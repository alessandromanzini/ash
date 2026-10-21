module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:dispatch;

import :codec;
import :policy;
import :write_config;
import :log_stream;


namespace ash::detail
{
   /**
    * Rebuilds a burnt argument record into text on the worker thread. This type is needed the dispatcher can erase arguments' types when staging
    * the message. See \c render_burnt for more details.
    */
   using render_fn_type = auto ( * )( std::string& out, std::string_view fmt, std::span<std::byte const> payload ) noexcept -> void;

   /**
    * A deferred log record: metadata plus the burnt format arguments, held in a fixed inline buffer.
    * @note Aligned to \c std::hardware_destructive_interference_size to avoid false sharing.
    * @note This costs nothing as long as \p N is chosen so the record lands on a multiple of the interference size... the header (metadata, renderer,
    *       format view and flags) occupies 51 bytes and \p N = 205 by default, which yields exactly 256.
    */
   template <std::size_t N = 205> struct alignas( std::hardware_destructive_interference_size ) Message
   {
      static constexpr std::size_t capacity{ N };

      schema::WriteMetadata metadata;
      render_fn_type render_fn;
      std::string_view fmt;
      std::uint16_t body_len;
      bool truncated;
      std::byte body[N];
   };
   static_assert( std::is_trivially_copyable_v<Message<>>, "Message must be trivially copyable." );
   static_assert( sizeof( Message<> ) == 256, "Default Message capacity no longer packs the record into 256 bytes." );
}

namespace ash::detail
{
   /**
    * Render \p fmt + \p args into the contiguous \p dst. Writing into a raw buffer keeps std::format on libc++'s direct (non-allocating) output
    * path, and format_to_n reports the full length even when it does not fit, so the caller learns whether it was truncated.
    *
    * @return { bytes written, truncated }.
    */
   template <typename... Args>
   CBR_FORCE_INLINE auto render_to_buf( std::span<char> dst, std::format_string<Args...> fmt, Args&&... args ) noexcept -> std::pair<std::size_t, bool>
   {
      auto const [out, needed] = std::format_to_n( dst.data( ), std::ssize( dst ), fmt, std::forward<Args>( args )... );
      return { static_cast<std::size_t>( out - dst.data( ) ), std::cmp_greater( needed, dst.size( ) ) };
   }

   /**
    * Worker-side renderer for a record burnt by \p BurnerT. The \c BurnerT type is erased by storing the function as a pointer inside the Message
    * record.
    */
   template <typename BurnerT> auto burn_to_str( std::string& out, std::string_view fmt, std::span<std::byte const> payload ) noexcept -> void
   {
      out.clear( );
      BurnerT::feed( payload, [&]( auto const&... values ) {
         std::vformat_to( std::back_inserter( out ), fmt, std::make_format_args( values... ) );
      } );
   }
}

export namespace ash
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
         : stream_{ std::move( config ) }
      { }

      template <typename... Args>
      CBR_FORCE_INLINE auto dispatch( schema::WriteMetadata const& metadata, std::format_string<Args...> fmt, Args&&... args ) const noexcept -> void
      {
         std::array<char, P::inline_buffer_size> stack_buffer;
         auto const [written, truncated] = detail::render_to_buf( stack_buffer, fmt, std::forward<Args>( args )... );
         //
         if constexpr ( P::overflow_policy == policy::OverflowPolicy::truncate )
         {
            // "truncate" policy: emit the bytes that fit the stack budget, regardless of truncation.
            stream_.write<P::inline_buffer_size>( metadata, std::string_view{ stack_buffer.data( ), written } );
         }
         else if constexpr ( P::overflow_policy == policy::OverflowPolicy::send_to_heap )
         {
            // "send_to_heap" policy: if truncation happens, resort to std::format.
            if ( not truncated ) [[likely]]
            {
               stream_.write<P::inline_buffer_size>( metadata, std::string_view{ stack_buffer.data( ), written } );
            }
            else
            {
               stream_.write<P::inline_buffer_size>( metadata, std::format( fmt, std::forward<Args>( args )... ) );
            }
         }
      }

   private:
      LogStream stream_;
   };

   /**
    * Deferred dispatch: chunks are queued in a ring buffer and drained on a dedicated worker thread.
    * The ring is a lock-free SPSC queue: the producer owns the "tail", the consumer owns the "head", and the payload is published/observed through
    * release/acquire ordering. The producer never wakes the worker: the worker polls, yielding briefly after activity and otherwise sleeping
    * \c P::poll_interval between checks, so logging never costs the caller a system call.
    *
    * Format arguments are not rendered on the caller thread: they are burnt into the slot through their \c traits::Codec, and the worker formats
    * them. String arguments are copied, so nothing in the record refers back to the caller's storage.
    *
    * @note SPSC CONTRACT: there must be at most one producer at a time.
    */
   template <traits::dispatch_policy P> requires( P::dispatch_timing == policy::DispatchTiming::deferred )
   class Dispatcher<P> final
   {
      static_assert( P::retention_policy == policy::RetentionPolicy::wine, "Deferred Dispatcher only supports 'wine' retention policy." );
      static_assert( P::overflow_policy == policy::OverflowPolicy::truncate, "Deferred Dispatcher only supports 'truncate' overflow policy." );

      static constexpr std::size_t message_size{ P::inline_buffer_size == 0 ? detail::Message<>::capacity : P::inline_buffer_size };
      using message_type = detail::Message<message_size>;
      using ring_type = std::array<message_type, P::pool_size>;

      // Empty polls the worker spends yielding before it sleeps, so a burst is drained continuously instead of once per poll interval.
      static constexpr std::size_t idle_spin_limit{ 64 };

      // Producer-owned and consumer-owned state live on separate cache lines, so neither side's writes evict the other's.
      static constexpr std::size_t cache_line{ std::hardware_destructive_interference_size };

   public:
      explicit Dispatcher( schema::WriteConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      ~Dispatcher( ) noexcept
      {
         // Request stop, then join so the ring is torn down only after the worker has fully drained it. A sleeping worker notices within one poll
         // interval.
         // TODO: use std::jthread + std::stop_token once module consumers link against it: at -O0, clang does not emit libc++'s
         //       std::__atomic_unique_lock::__set_locked_bit for importers, leaving it undefined. (last checked on clang 22.1.8)
         request_stop( );
         worker_.join( );
      }

      Dispatcher( Dispatcher const& ) noexcept = delete;
      Dispatcher( Dispatcher&& ) noexcept = delete;
      auto operator=( Dispatcher const& ) noexcept -> Dispatcher& = delete;
      auto operator=( Dispatcher&& ) noexcept -> Dispatcher& = delete;

      /**
       * Burn the arguments into the ring slot; the worker formats them with \p fmt.
       * @note The format string must have static storage duration, which std::format_string guarantees for literals.
       */
      template <typename... Args>
      auto dispatch( schema::WriteMetadata const& metadata, std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         stage<std::remove_cvref_t<Args>...>( metadata, fmt.get( ), args... );
      }

   private:
      // Read-only once the worker runs.
      schema::WriteConfig const config_;

      // On the heap, so the dispatcher stays small enough to live on any thread's stack.
      // We leave it uninitialized because every slot is fully written before it is published, and the worker never reads body bytes past body_len.
      std::unique_ptr<ring_type> const ring_{ std::make_unique_for_overwrite<ring_type>( ) };

      // PRODUCER line: its write position, and its last known copy of the consumer's position.
      alignas( cache_line ) std::atomic<std::size_t> tail_{ 0 };
      std::size_t head_cache_{ 0 };

      // CONSUMER line: its read position.
      alignas( cache_line ) std::atomic<std::size_t> head_{ 0 };

      // Written once by the destructor, read on every poll.
      alignas( cache_line ) std::atomic_flag stop_token_{};

      std::thread worker_{ [this] { run( ); } };

      /**
       * Burn the arguments into the next ring slot on the caller thread, then publish it.
       */
      template <typename... Ts> CBR_FORCE_INLINE auto stage( schema::WriteMetadata const& metadata, std::string_view fmt, Ts const&... args ) noexcept -> void
      {
         using burner_type = Burner<Ts...>;
         static_assert( burner_type::fixed_footprint <= message_type::capacity, "Fixed-size arguments exceed the message capacity." );
         //
         std::size_t const tail{ tail_.load( std::memory_order_relaxed ) };
         //
         // 1. Only read the consumer's line when the cached head says the ring may be full, then drop the newest entry if it really is (wine
         //    retention).
         if ( std::cmp_equal( tail - head_cache_, P::pool_size ) )
         {
            head_cache_ = head_.load( std::memory_order_acquire );
            if ( std::cmp_equal( tail - head_cache_, P::pool_size ) )
            {
               return;
            }
         }
         //
         // 2. Fill the slot, then publish it with a release store on tail_.
         message_type& slot{ slot_at( tail ) };
         {
            auto const [written, truncated] = burner_type::burn( slot.body, args... );
            slot.body_len = static_cast<std::uint16_t>( written );
            slot.truncated = truncated;
         }
         slot.render_fn = &detail::burn_to_str<burner_type>;
         slot.fmt = fmt;
         slot.metadata = metadata;
         //
         tail_.store( tail + 1, std::memory_order_release );
      }

      auto run( ) noexcept -> void
      {
         LogStream const stream{ config_ };
         //
         std::string scratch{};
         scratch.reserve( 512 );
         std::size_t idle_spins{ 0 };
         //
         while ( true )
         {
            // 1. Process whatever has been published so far.
            if ( drain( stream, scratch ) > 0 )
            {
               idle_spins = 0;
               continue;
            }
            //
            // 2. Stop only once stop was requested (destructor) and the ring is drained. Stop is requested after the last publish, so one final
            //    drain observes everything.
            if ( is_stop_requested( ) )
            {
               drain( stream, scratch );
               break;
            }
            //
            // 3. Right after activity, yield instead of sleeping: a burst then keeps being drained as it arrives.
            if ( idle_spins < idle_spin_limit )
            {
               ++idle_spins;
               std::this_thread::yield( );
               continue;
            }
            //
            // 4. Idle: sleep until the next poll. Nobody wakes the worker early, so the producer never pays for it.
            std::this_thread::sleep_for( P::poll_interval );
         }
      }

      /**
       * Write every published slot from head to tail, then free them for the producer.
       * @return The number of slots written.
       */
      auto drain( LogStream const& stream, std::string& scratch ) noexcept -> std::size_t
      {
         std::size_t const first{ head_.load( std::memory_order_relaxed ) };
         std::size_t const tail{ tail_.load( std::memory_order_acquire ) };
         //
         for ( std::size_t head{ first }; head != tail; ++head )
         {
            message_type const& slot{ slot_at( head ) };
            slot.render_fn( scratch, slot.fmt, { slot.body, slot.body_len } );
            stream.write<message_size>( slot.metadata, scratch );
         }
         //
         head_.store( tail, std::memory_order_release );
         return tail - first;
      }

      CBR_FORCE_INLINE auto slot_at( std::size_t index ) noexcept -> message_type& { return ( *ring_ )[index % P::pool_size]; }
      CBR_FORCE_INLINE auto slot_at( std::size_t index ) const noexcept -> message_type const& { return ( *ring_ )[index % P::pool_size]; }

      auto request_stop( ) noexcept -> void { stop_token_.test_and_set( std::memory_order_release ); }
      auto is_stop_requested( ) const noexcept -> bool { return stop_token_.test( std::memory_order_acquire ); }
   };
}
