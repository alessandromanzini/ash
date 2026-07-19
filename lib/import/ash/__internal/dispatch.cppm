module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:dispatch;

import :policy;
import :write_config;
import :writer;


namespace ash::detail
{
   /**
    * A fully rendered log record: metadata plus the formatted body, held in a fixed inline buffer.
    * @note
    */
   template <size_t N> struct alignas( 64 ) Message
   {
      schema::WriteMetadata metadata;
      bool truncated;
      uint16_t body_len;
      char body[N];
   };
   static_assert( std::is_trivially_copyable_v<Message<1>>, "Message must be trivially copyable." );

   /**
    * Render \p fmt + \p args into the contiguous \p dst. Writing into a raw buffer keeps std::format on libc++'s direct (non-allocating) output
    * path, and format_to_n reports the full length even when it does not fit, so the caller learns whether it was truncated.
    *
    * @return { bytes written, truncated }.
    */
   template <typename... Args>
   CBR_FORCE_INLINE auto render_to( std::span<char> dst, std::format_string<Args...> fmt, Args&&... args ) noexcept -> std::pair<size_t, bool>
   {
      auto const [out, needed] = std::format_to_n( dst.data( ), std::ssize( dst ), fmt, std::forward<Args>( args )... );
      return { static_cast<size_t>( out - dst.data( ) ), std::cmp_greater( needed, dst.size( ) ) };
   }
}

namespace ash::detail
{
   template <traits::dispatch_policy P> class Dispatcher;

   /**
    * @brief Direct dispatch: render and write on the caller thread.
    */
   template <traits::dispatch_policy P> requires( P::dispatch_timing == policy::DispatchTiming::direct )
   class Dispatcher<P> final
   {
   public:
      explicit Dispatcher( schema::WriteConfig config ) noexcept
         : writer_{ std::move( config ) }
      { }

      template <typename... Args>
      CBR_FORCE_INLINE auto dispatch( schema::WriteMetadata const& metadata, std::format_string<Args...> fmt, Args&&... args ) const noexcept -> void
      {
         std::array<char, P::inline_buffer_size> stack_buffer;
         auto const [written, truncated] = detail::render_to( stack_buffer, fmt, std::forward<Args>( args )... );
         //
         if constexpr ( P::overflow_policy == policy::OverflowPolicy::truncate )
         {
            // "truncate" policy: emit the bytes that fit the stack budget, regardless of truncation.
            writer_.emit<P::inline_buffer_size>( metadata, std::string_view{ stack_buffer.data( ), written } );
         }
         else if constexpr ( P::overflow_policy == policy::OverflowPolicy::send_to_heap )
         {
            // "send_to_heap" policy: if truncation happens, resort to std::format.
            if ( not truncated ) [[likely]]
            {
               writer_.emit<P::inline_buffer_size>( metadata, std::string_view{ stack_buffer.data( ), written } );
            }
            else
            {
               writer_.emit<P::inline_buffer_size>( metadata, std::format( fmt, std::forward<Args>( args )... ) );
            }
         }
      }

      CBR_FORCE_INLINE auto dispatch( schema::WriteMetadata const& metadata, std::string_view body ) const noexcept -> void
      {
         writer_.emit<P::inline_buffer_size>( metadata, body );
      }

   private:
      Writer writer_;
   };

   /**
    * Deferred dispatch: chunks are queued in a ring buffer and drained on a dedicated worker thread.
    * The ring is a lock-free SPSC queue: the producer owns the "tail", the consumer owns the "head", and the payload is published/observed through
    * release/acquire ordering. Blocking is done with an atomic doorbell ("wait"/"notify").
    *
    * @note SPSC CONTRACT: there must be at most one producer at a time.
    */
   template <traits::dispatch_policy P> requires( P::dispatch_timing == policy::DispatchTiming::deferred )
   class Dispatcher<P> final
   {
      static_assert( P::retention_policy == policy::RetentionPolicy::wine, "Deferred Dispatcher only supports 'wine' retention policy." );
      static_assert( P::overflow_policy == policy::OverflowPolicy::truncate, "Deferred Dispatcher only supports 'truncate' overflow policy." );

      using message_type = Message<P::inline_buffer_size>;

   public:
      explicit Dispatcher( schema::WriteConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      ~Dispatcher( ) noexcept
      {
         // Request stop so the worker can break out of the loop, ring the doorbell to wake it, then join so the ring is torn down only after it has
         // fully drained.
         // TODO: use std::stop_token when non-experimental in clang libc++. (last checked on 22.1.8)
         request_stop( );
         ring_doorbell( );
         worker_.join( );
      }

      Dispatcher( Dispatcher const& ) noexcept = delete;
      Dispatcher( Dispatcher&& ) noexcept = delete;
      auto operator=( Dispatcher const& ) noexcept -> Dispatcher& = delete;
      auto operator=( Dispatcher&& ) noexcept -> Dispatcher& = delete;

      template <typename... Args>
      auto dispatch( schema::WriteMetadata const& metadata, std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         stage( metadata, [&]( std::span<char> dst ) { return detail::render_to( dst, fmt, std::forward<Args>( args )... ); } );
      }

      // Pre-rendered overload: copy the finished body straight into the slot (truncating to capacity).
      auto dispatch( schema::WriteMetadata const& metadata, std::string_view body ) noexcept -> void
      {
         stage(
           metadata, [&]( std::span<char> dst ) { return std::make_pair( body.copy( dst.data( ), dst.size( ) ), body.size( ) > dst.size( ) ); } );
      }

   private:
      schema::WriteConfig const config_;

      std::array<message_type, P::pool_size> ring_{};
      std::atomic<size_t> head_{ 0 };       // Consumer-owned read index (monotonic) -> read by the producer for fullness
      std::atomic<size_t> tail_{ 0 };       // Producer-owned write index (monotonic) -> read by the consumer for availability
      std::atomic<uint32_t> doorbell_{ 0 }; // Bumped + notified on publish and on stop
      std::atomic_flag stop_token_{};

      std::thread worker_{ [this] { run( ); } };

      /**
       * Render straight into the next ring slot on the caller thread, then publish it.
       */
      template <std::invocable<std::span<char>> F>
      CBR_FORCE_INLINE auto stage( schema::WriteMetadata const& metadata, F render_body_fn ) noexcept -> void
      {
         size_t const tail = tail_.load( std::memory_order_relaxed );
         //
         // 1. Drop the newest entry if the ring is full (wine retention).
         if ( bool const pool_congested = tail - head_.load( std::memory_order_acquire ) == P::pool_size; pool_congested )
         {
            return;
         }
         //
         // 2. Render into the slot, then publish it with a release store on tail_.
         message_type& slot = slot_at( tail );
         {
            auto const [written, truncated] = std::invoke( render_body_fn, std::span<char>{ slot.body } );
            slot.body_len = static_cast<uint16_t>( written );
            slot.truncated = truncated;
         }
         slot.metadata = metadata;
         //
         tail_.store( tail + 1, std::memory_order_release );
         //
         // 3. Ring the doorbell so the worker wakes if it was sleeping.
         ring_doorbell( );
      }

      auto run( ) noexcept -> void
      {
         Writer const writer{ config_ };
         //
         while ( true )
         {
            // 1. Snapshot the doorbell BEFORE draining, so a publishing that races the drain still wakes us on the next wait.
            uint32_t const ticket = doorbell_.load( std::memory_order_acquire );
            //
            // 2. Process queue head to tail.
            size_t head = head_.load( std::memory_order_relaxed );
            size_t const tail = tail_.load( std::memory_order_acquire );
            while ( head != tail )
            {
               message_type const& slot = slot_at( head );
               writer.emit<P::inline_buffer_size>( slot.metadata, std::string_view{ slot.body, slot.body_len } );
               ++head;
            }
            //
            // 3. Free the drained slots for the producer.
            head_.store( head, std::memory_order_release );
            //
            // 4. Stop only once stop was requested (destructor) and the ring is drained; otherwise hold until the next doorbell ring.
            if ( bool const pool_drained = head == tail_.load( std::memory_order_acquire ); is_stop_requested( ) && pool_drained )
            {
               break;
            }
            doorbell_.wait( ticket, std::memory_order_acquire );
         }
      }

      CBR_FORCE_INLINE auto slot_at( size_t index ) noexcept -> message_type& { return ring_[index % P::pool_size]; }
      CBR_FORCE_INLINE auto slot_at( size_t index ) const noexcept -> message_type const& { return ring_[index % P::pool_size]; }

      auto ring_doorbell( ) noexcept -> void
      {
         doorbell_.fetch_add( 1, std::memory_order_release );
         doorbell_.notify_one( );
      }

      auto request_stop( ) noexcept -> void { stop_token_.test_and_set( std::memory_order_release ); }
      auto is_stop_requested( ) const noexcept -> bool { return stop_token_.test( std::memory_order_acquire ); }
   };
}
