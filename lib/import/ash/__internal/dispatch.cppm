module;

#include <ash/pch.hpp>

#include <bhop/cbridge.hpp>

export module ash:dispatch;

import bhop;

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
    * @note The header (metadata, renderer, format view and flags) occupies 51 bytes. The default capacities below make each ring slot exactly 256
    *       bytes: 205 in a plain slot, 197 next to the sequence number of the ordered ring's slots.
    */
   template <std::size_t N> struct Message
   {
      static constexpr std::size_t capacity{ N };

      schema::WriteMetadata metadata;
      render_fn_type render_fn;
      std::string_view fmt;
      std::uint16_t body_len;
      bool truncated;
      std::byte body[N];
   };

   inline constexpr std::size_t plain_message_capacity{ 205 };
   inline constexpr std::size_t sequenced_message_capacity{ 197 };

   static_assert( std::is_trivially_copyable_v<Message<plain_message_capacity>>, "Message must be trivially copyable." );
   static_assert( bhop::SpscRing<Message<plain_message_capacity>, 1>::slot_size == 256, "Default plain slot no longer packs into 256 bytes." );
   static_assert( bhop::MpscRing<Message<sequenced_message_capacity>, 1>::slot_size == 256, "Default sequenced slot no longer packs into 256 bytes." );

   template <traits::dispatch_policy D>
   using plain_message_t = Message<D::inline_buffer_size == 0 ? plain_message_capacity : D::inline_buffer_size>;

   template <traits::dispatch_policy D>
   using sequenced_message_t = Message<D::inline_buffer_size == 0 ? sequenced_message_capacity : D::inline_buffer_size>;

   /**
    * The ring a deferred dispatcher uses: a single SPSC ring when only one thread logs, otherwise one ring per thread, or one shared ring when
    * exact order was requested.
    */
   template <traits::dispatch_policy D, traits::access_policy A>
   using deferred_ring_t = std::conditional_t<not A::multi_producer, bhop::SpscRing<plain_message_t<D>, D::pool_size>,
                                              std::conditional_t<D::ordered, bhop::MpscRing<sequenced_message_t<D>, D::pool_size>,
                                                                 bhop::ThreadRings<plain_message_t<D>, D::pool_size>>>;
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
   CBR_FORCE_INLINE auto render_to_buf( std::span<char> dst, std::format_string<Args...> fmt, Args&&... args ) noexcept
     -> std::pair<std::size_t, bool>
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
      BurnerT::feed(
        payload, [&]( auto const&... values ) { std::vformat_to( std::back_inserter( out ), fmt, std::make_format_args( values... ) ); } );
   }

   /**
    * Burn the arguments into \p message on the caller thread.
    */
   template <std::size_t N, typename... Ts>
   CBR_FORCE_INLINE auto fill( Message<N>& message, schema::WriteMetadata const& metadata, std::string_view fmt, Ts const&... args ) noexcept -> void
   {
      using burner_type = Burner<Ts...>;
      static_assert( burner_type::fixed_footprint <= N, "Fixed-size arguments exceed the message capacity." );
      //
      auto const [written, truncated] = burner_type::burn( message.body, args... );
      message.body_len = static_cast<std::uint16_t>( written );
      message.truncated = truncated;
      message.render_fn = &burn_to_str<burner_type>;
      message.fmt = fmt;
      message.metadata = metadata;
   }
}

export namespace ash
{
   template <traits::dispatch_policy D, traits::access_policy A> class Dispatcher;

   /**
    * @brief Direct dispatch: render and write on the caller thread.
    *
    * @note If sync, only the actual write to stream is lock-guarded.
    */
   template <traits::dispatch_policy D, traits::access_policy A> requires( D::dispatch_timing == policy::DispatchTiming::direct )
   class Dispatcher<D, A> final
   {
   public:
      explicit Dispatcher( schema::WriteConfig config ) noexcept
         : stream_{ std::move( config ) }
      { }

      template <typename... Args>
      CBR_FORCE_INLINE auto dispatch( schema::WriteMetadata metadata, std::format_string<Args...> fmt, Args&&... args ) const noexcept -> void
      {
         // 1. Apply the format on this thread's stack outside the lock.
         std::array<char, D::inline_buffer_size> stack_buffer;
         auto const [written, truncated] = detail::render_to_buf( stack_buffer, fmt, std::forward<Args>( args )... );
         //
         // 2. Write to stream, which is the critical section. The title's tick anchor and the sequence counter are what concurrent callers share.
         if constexpr ( D::overflow_policy == policy::OverflowPolicy::truncate )
         {
            // "truncate" policy: emit the bytes that fit the stack budget, regardless of truncation.
            write( metadata, std::string_view{ stack_buffer.data( ), written } );
         }
         else if constexpr ( D::overflow_policy == policy::OverflowPolicy::send_to_heap )
         {
            // "send_to_heap" policy: if truncation happens, resort to std::format, still before taking the lock.
            if ( not truncated ) [[likely]]
            {
               write( metadata, std::string_view{ stack_buffer.data( ), written } );
            }
            else
            {
               write( metadata, std::format( fmt, std::forward<Args>( args )... ) );
            }
         }
      }

   private:
      LogStream stream_;
      mutable A::lock_type write_lock_{};
      mutable schema::WriteMetadata::sequence_id_type next_sequence_{ 0 };

      CBR_FORCE_INLINE auto write( schema::WriteMetadata& metadata, std::string_view body ) const noexcept -> void
      {
         std::lock_guard lock{ write_lock_ };
         metadata.sequence_id = next_sequence_++;
         stream_.write<D::inline_buffer_size>( metadata, body );
      }
   };

   /**
    * Deferred dispatch: entries are queued in a ring and drained on a dedicated worker thread. The worker polls on a \c D::poll_interval, yielding
    * briefly after activity, so logging never costs the caller a system call.
    *
    * Format arguments are not rendered on the caller thread: they are burnt into the slot through their \c traits::Codec, and the worker formats
    * them. String arguments are copied, so nothing in the record refers back to the caller's storage (safe out-of-scope arguments).
    *
    * The ring depends on the policies (see \c detail::deferred_ring_t): one SPSC ring for a single-threaded access policy; with a multithreaded
    * one, a ring per thread (no contention) or, when \c D::ordered, one shared lock-free ring (exact call order).
    */
   template <traits::dispatch_policy D, traits::access_policy A> requires( D::dispatch_timing == policy::DispatchTiming::deferred )
   class Dispatcher<D, A> final
   {
      static_assert( D::retention_policy == policy::RetentionPolicy::wine, "Deferred Dispatcher only supports 'wine' retention policy." );
      static_assert( D::overflow_policy == policy::OverflowPolicy::truncate, "Deferred Dispatcher only supports 'truncate' overflow policy." );

      using ring_type = detail::deferred_ring_t<D, A>;
      using message_type = ring_type::value_type;

      // Empty polls the worker spends yielding before it sleeps, so a burst is drained continuously instead of once per poll interval.
      static constexpr std::size_t idle_spin_limit{ 64 };

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
       * Burn the arguments into a ring slot; the worker formats them with \p fmt.
       * @note The format string must have static storage duration, which std::format_string guarantees for literals.
       */
      template <typename... Args>
      auto dispatch( schema::WriteMetadata const& metadata, std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         // A full ring drops the newest entry ('wine' retention).
         std::ignore = ring_.try_push( [&]( message_type& message ) {
            detail::fill<message_type::capacity, std::remove_cvref_t<Args>...>( message, metadata, fmt.get( ), args... );
         } );
      }

   private:
      schema::WriteConfig const config_;
      ring_type ring_{};

      // Written once by the destructor, read on every poll.
      alignas( bhop::traits::cache_line_size_v ) std::atomic_flag stop_token_{};

      std::thread worker_{ [this] { run( ); } };

      auto run( ) noexcept -> void
      {
         LogStream const stream{ config_ };
         //
         std::string scratch{};
         scratch.reserve( 512 );
         schema::WriteMetadata::sequence_id_type sequence{ 0 };
         std::size_t idle_spins{ 0 };
         //
         // Entries are numbered in the order they are written.
         auto write = [&]( message_type const& message ) {
            message.render_fn( scratch, message.fmt, { message.body, message.body_len } );
            schema::WriteMetadata metadata{ message.metadata };
            metadata.sequence_id = sequence++;
            stream.write<message_type::capacity>( metadata, scratch );
         };
         //
         while ( true )
         {
            // 1. Process whatever has been published so far.
            if ( drain( write ) > 0 )
            {
               idle_spins = 0;
               continue;
            }
            //
            // 2. Stop only once stop was requested (destructor) and the ring is drained. Stop is requested after the last publish, so one final
            //    drain observes everything.
            if ( is_stop_requested( ) )
            {
               drain( write );
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
            std::this_thread::sleep_for( D::poll_interval );
         }
      }

      /**
       * Per-thread rings are merged by timestamp; the other rings already hold a single order.
       */
      template <typename F> auto drain( F& write ) noexcept -> std::size_t
      {
         struct ByTimestamp
         {
            [[nodiscard]] auto operator( )( message_type const& message ) const noexcept
            {
               return message.metadata.timestamp;
            }
         };
         //
         if constexpr ( requires { ring_.drain( write, ByTimestamp{} ); } )
         {
            return ring_.drain( write, ByTimestamp{} );
         }
         else
         {
            return ring_.drain( write );
         }
      }

      auto request_stop( ) noexcept -> void { stop_token_.test_and_set( std::memory_order_release ); }
      auto is_stop_requested( ) const noexcept -> bool { return stop_token_.test( std::memory_order_acquire ); }
   };
}
