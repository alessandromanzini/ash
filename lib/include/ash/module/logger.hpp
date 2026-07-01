#ifndef ASH_LOGGER_HPP
#define ASH_LOGGER_HPP

#include <ash/pch.hpp>

#include <array>
#include <ash/config/signal.hpp>
#include <ash/module/policy.hpp>


namespace ash
{
   struct LogConfig
   {
      std::string                          identity   = "LOGGER";
      std::reference_wrapper<std::ostream> log_stream = std::clog;
      std::reference_wrapper<std::ostream> err_stream = std::cerr;
   };
}

namespace ash::detail
{
   struct LogOptions
   {
      std::optional<uint32_t> echo;
      std::source_location    location;
   };

   struct alignas( 64 ) LogEntry
   {
      enum class ChunkPosition : uint8_t { sole, head, body, tail };

      using chunk_id_type    = uint16_t;
      using chunk_size_type  = uint16_t;
      using sequence_id_type = uint32_t;

      std::time_t      timestamp;
      LogOptions       options;
      Signal           signal;
      ChunkPosition    chunk_position;
      chunk_id_type    chunk_id;
      sequence_id_type sequence_id;
      chunk_size_type  chunk_len;
      char             msg[222];
   };
   static_assert( alignof( LogEntry ) == 64 && sizeof( LogEntry ) % 64 == 0 );
}

namespace ash::detail
{
   enum class FlushAction : uint8_t { carry_on, end };

   class SequentialChunker final
   {
      class Iterator final
      {
      public:
         using iterator_category = std::output_iterator_tag;
         using value_type        = void;
         using difference_type   = std::ptrdiff_t;
         using pointer           = void;
         using reference         = void;

         explicit Iterator( SequentialChunker& chunker, LogEntry& entry )
            : chunker_ptr_{ &chunker }
            , entry_ptr_{ &entry }
         { }

         auto operator*( ) noexcept -> Iterator& { return *this; }
         auto operator++( ) noexcept -> Iterator& { return *this; }
         auto operator++( int ) const noexcept -> Iterator { return *this; }

         auto operator=( char const c ) noexcept -> Iterator&
         {
            LogEntry::chunk_size_type& buffer_pos = entry_ptr_->chunk_len;
            if ( constexpr size_t max_msg_size = sizeof( LogEntry::msg ) - 1ULL; buffer_pos == max_msg_size )
            {
               flush( FlushAction::carry_on );
            }
            entry_ptr_->msg[buffer_pos++] = c;
            //
            return *this;
         }

         auto flush( FlushAction action ) const noexcept -> void
         {
            entry_ptr_->chunk_position = [this, action] -> LogEntry::ChunkPosition {
               bool const is_first = entry_ptr_->chunk_id == 0;
               bool const is_last  = action == FlushAction::end;
               if ( is_first && is_last )
               {
                  return LogEntry::ChunkPosition::sole;
               }
               if ( is_first )
               {
                  return LogEntry::ChunkPosition::head;
               }
               if ( is_last )
               {
                  return LogEntry::ChunkPosition::tail;
               }
               return LogEntry::ChunkPosition::body;
            }( );
            //
            entry_ptr_->chunk_id++;
            entry_ptr_->msg[entry_ptr_->chunk_len++] = '\0';
            //
            std::invoke( chunker_ptr_->flush_fn_, *entry_ptr_ );
            entry_ptr_->chunk_len = 0;
         }

      private:
         SequentialChunker* chunker_ptr_ = nullptr;
         LogEntry*          entry_ptr_   = nullptr;
      };

   public:
      using flush_fn_type = std::function<void( LogEntry const& )>; // This should be std::move_only_function when clang supports it :)

      explicit SequentialChunker( Signal sig, LogOptions const& options, LogEntry::sequence_id_type seq_id, flush_fn_type flush_fn ) noexcept
         : flush_fn_{ std::move( flush_fn ) }
         , current_chunk_{ .timestamp      = std::time( nullptr ),
                           .options        = options,
                           .signal         = sig,
                           .chunk_position = LogEntry::ChunkPosition::head,
                           .chunk_id       = 0,
                           .sequence_id    = seq_id,
                           .chunk_len      = 0,
                           .msg            = {} }
      { }

      auto iterator( ) noexcept -> Iterator { return Iterator{ *this, current_chunk_ }; }

   private:
      flush_fn_type const flush_fn_{};
      LogEntry            current_chunk_{};
   };
}

namespace ash::detail
{
   class LogWriter final
   {
      enum class Header : uint8_t { skip, write };
      enum class Trailer : uint8_t { skip, write };

   public:
      explicit LogWriter( LogConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      auto consume( LogEntry const& entry ) noexcept -> void
      {
         switch ( entry.chunk_position )
         {
            case LogEntry::ChunkPosition::sole: emit( entry, Header::write, Trailer::write ); return;
            case LogEntry::ChunkPosition::head: emit( entry, Header::write, Trailer::skip ); return;
            case LogEntry::ChunkPosition::body: emit( entry, Header::skip, Trailer::skip ); return;
            case LogEntry::ChunkPosition::tail: emit( entry, Header::skip, Trailer::write ); return;
            default                           : std::unreachable( );
         }
      }

   private:
      LogConfig const config_;

      /**
       * Route log stream and err stream depending on the input signal.
       * LOG: trace -> warning
       * ERR: error & fatal
       * @param sig
       */
      [[nodiscard]] auto select_sink( Signal sig ) const noexcept -> std::ostream&
      {
         return std::greater_equal<Signal>{}( sig, Signal::error ) ? config_.err_stream : config_.log_stream;
      }

      auto emit( LogEntry const& entry, Header header, Trailer trailer ) const noexcept -> void
      {
         // Get the stream's buffer directly so we can format directly to it without any intermediate std::string.
         std::streambuf* const buf = select_sink( entry.signal ).rdbuf( );
         //
         if ( header == Header::write )
         {
            std::source_location const& loc = entry.options.location;
            constexpr auto              fmt = "[{:u}] [{}] at {}:{}:{}\n";
            std::format_to( std::ostreambuf_iterator{ buf }, fmt, entry.signal, config_.identity, loc.file_name( ), loc.line( ), loc.column( ) );
         }
         //
         buf->sputn( entry.msg, entry.chunk_len > 0 ? entry.chunk_len - 1 : 0 );
         //
         if ( trailer == Trailer::write )
         {
            buf->sputn( "\n\n", 2 );
         }
      }
   };

   /**
    * Direct dispatch: consume the entries on the caller thread.
    */
   class DirectDispatcher final
   {
   public:
      explicit DirectDispatcher( LogConfig config ) noexcept
         : writer_{ std::move( config ) }
      { }

      auto dispatch( LogEntry const& entry ) noexcept -> void { writer_.consume( entry ); }

   private:
      LogWriter writer_;
   };

   /**
    * Deferred dispatch: chunks are queued in a ring buffer and drained on a dedicated worker thread.
    * The ring is a lock-free SPSC queue: the producer owns the "tail", the consumer owns the "head", and the payload is published/observed through
    * release/acquire ordering. Blocking is done with an atomic doorbell ("wait"/"notify").
    *
    * @note SPSC CONTRACT: there must be at most one producer at a time.
    *
    * @tparam pool_size The maximum queue capacity. When the queue is full, it discards the newest entry (wine policy).
    */
   template <size_t pool_size> class DeferredDispatcher final
   {
   public:
      explicit DeferredDispatcher( LogConfig config ) noexcept
         : config_{ std::move( config ) }
      { }

      ~DeferredDispatcher( ) noexcept
      {
         // Request stop so the worker can break out of the loop and update and ring the doorbell to resume worker execution. worker_ is the last
         // member, so its join runs before the ring is torn down.
         worker_.request_stop( );
         ring_doorbell( );
      }

      DeferredDispatcher( DeferredDispatcher const& ) noexcept                    = delete;
      DeferredDispatcher( DeferredDispatcher&& ) noexcept                         = delete;
      auto operator=( DeferredDispatcher const& ) noexcept -> DeferredDispatcher& = delete;
      auto operator=( DeferredDispatcher&& ) noexcept -> DeferredDispatcher&      = delete;

      auto dispatch( LogEntry const& entry ) noexcept -> void
      {
         size_t const tail = tail_.load( std::memory_order_relaxed );
         //
         // 1. Drop the newest entry if the ring is full.
         if ( tail - head_.load( std::memory_order_acquire ) == pool_size )
         {
            return;
         }
         //
         // 2. Write the slot, then publish it with a release store on tail_.
         ring_[tail % pool_size] = entry;
         tail_.store( tail + 1, std::memory_order_release );
         //
         // 3. Ring the doorbell so the worker wakes if it was sleeping.
         ring_doorbell( );
      }

   private:
      LogConfig const config_;

      std::array<LogEntry, pool_size> ring_{};
      std::atomic<size_t>             head_{ 0 };     // Consumer-owned read index (monotonic) -> read by the producer for fullness
      std::atomic<size_t>             tail_{ 0 };     // Producer-owned write index (monotonic) -> read by the consumer for availability
      std::atomic<uint32_t>           doorbell_{ 0 }; // Bumped + notified on publish and on stop

      std::jthread worker_{ [this]( std::stop_token const& stop ) { run( stop ); } };

      auto run( std::stop_token const& stop ) noexcept -> void
      {
         LogWriter writer{ config_ };
         //
         while ( true )
         {
            // 1. Read the doorbell BEFORE draining. This way we snapshot at which doorbell value we are before consuming the queue data.
            uint32_t const ticket = doorbell_.load( std::memory_order_acquire );
            //
            // 2. Process queue head to tail.
            size_t       head = head_.load( std::memory_order_relaxed );
            size_t const tail = tail_.load( std::memory_order_acquire );
            while ( head != tail )
            {
               writer.consume( ring_[head % pool_size] );
               ++head;
            }
            //
            // 3. Free the drained slots for the producer.
            head_.store( head, std::memory_order_release );
            //
            // 4. Break the loop if stop was requested (can only happen from the destructor) and pool is drained, otherwise hold until the next
            // doorbell ring.
            if ( bool const pool_drained = head == tail_.load( std::memory_order_acquire ); stop.stop_requested( ) && pool_drained )
            {
               break;
            }
            doorbell_.wait( ticket, std::memory_order_acquire );
         }
      }

      auto ring_doorbell( ) noexcept -> void
      {
         doorbell_.fetch_add( 1, std::memory_order_release );
         doorbell_.notify_one( );
      }
   };

   template <traits::dispatch_policy P>
   using DispatchEngine =
     std::conditional_t<P::dispatch_timing == policy::DispatchTiming::deferred, DeferredDispatcher<P::pool_size>, DirectDispatcher>;
}

namespace ash
{
   template <typename DispatchFn> class LoggerOperator final
   {
   public:
      LoggerOperator( DispatchFn dispatch_fn ) noexcept
         : dispatch_fn_{ dispatch_fn }
      { }

      template <typename... Args> auto trace( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::trace, fmt.get( ), std::forward<Args>( args )... );
      }

      auto trace( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::trace, view ); }

      template <typename... Args> auto info( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::info, fmt.get( ), std::forward<Args>( args )... );
      }

      auto info( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::info, view ); }

      template <typename... Args> auto debug( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::debug, fmt.get( ), std::forward<Args>( args )... );
      }

      auto debug( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::debug, view ); }

      template <typename... Args> auto warning( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::warning, fmt.get( ), std::forward<Args>( args )... );
      }

      auto warning( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::warning, view ); }

      template <typename... Args> auto error( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::error, fmt.get( ), std::forward<Args>( args )... );
      }

      auto error( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::error, view ); }

      template <typename... Args> auto fatal( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::fatal, fmt.get( ), std::forward<Args>( args )... );
      }

      auto fatal( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::fatal, view ); }

   private:
      DispatchFn const dispatch_fn_;
   };

   /**
    * @brief Thread-safe/unsync logger front-end combining an access policy and dispatch policy.
    *
    * @note Streams referenced by \c LogConfig must remain valid for the entire lifetime of \c Logger, including any deferred writes still pending
    *       on the worker thread at destruction time.
    *
    * @tparam AccessPolicy   Policy controlling synch vs unsynch access.
    * @tparam DispatchPolicy Policy controlling direct vs deferred dispatch.
    */
   template <traits::access_policy AccessPolicy, traits::dispatch_policy DispatchPolicy> class Logger final
   {
      static_assert( DispatchPolicy::retention_policy != policy::RetentionPolicy::milk, "Milk policy is not supported!" );

   public:
      explicit Logger( AccessPolicy access, DispatchPolicy dispatch, LogConfig config = {} ) noexcept
         : access_policy_{ std::move( access ) }
         , dispatch_policy_{ std::move( dispatch ) }
         , dispatch_engine_{ std::move( config ) }
      { }
      ~Logger( ) = default;

      Logger( Logger const& )                        = delete;
      Logger( Logger&& ) noexcept                    = delete;
      auto operator=( Logger const& ) -> Logger&     = delete;
      auto operator=( Logger&& ) noexcept -> Logger& = delete;

      auto emit( std::source_location loc = std::source_location::current( ) ) noexcept
      {
         return make_operator( { .echo = std::nullopt, .location = loc } );
      }

      auto once( std::source_location loc = std::source_location::current( ) ) noexcept { return make_operator( { .echo = 1, .location = loc } ); }

      auto at_most( uint32_t cap, std::source_location loc = std::source_location::current( ) ) noexcept
      {
         return make_operator( { .echo = cap, .location = loc } );
      }

      auto filter_eq( Signal layer ) noexcept -> void { set_filter( layer, Filter::Op::eq ); }
      auto filter_lt( Signal layer ) noexcept -> void { set_filter( layer, Filter::Op::lt ); }
      auto filter_lt_eq( Signal layer ) noexcept -> void { set_filter( layer, Filter::Op::lt_eq ); }
      auto filter_gt( Signal layer ) noexcept -> void { set_filter( layer, Filter::Op::gt ); }
      auto filter_gt_eq( Signal layer ) noexcept -> void { set_filter( layer, Filter::Op::gt_eq ); }

   private:
      [[no_unique_address]] AccessPolicy   access_policy_;
      [[no_unique_address]] DispatchPolicy dispatch_policy_;

      detail::DispatchEngine<DispatchPolicy> dispatch_engine_;
      uint32_t                               sequence_counter_{ 0U };

      struct Filter
      {
         Signal layer                                               = Signal::trace;
         enum class Op : uint8_t { eq, lt, lt_eq, gt, gt_eq } order = Op::gt_eq;
      } filter_{};

      auto make_operator( detail::LogOptions const& options ) noexcept
      {
         return LoggerOperator{ [this, options]<typename... Args>( Signal sig, std::string_view fmt_view, Args&&... args ) {
            if ( not can_dispatch( sig ) )
            {
               return;
            }
            //
            // In case of a deferred dispatch, we must guarantee the SPSC CONTRACT ...
            // A. UNSYNC access policy: we only ever access the engine from the same thread, no issues here.
            // B. SYNC access policy: the gate mutex ensures the contract.
            access_policy_.gate( [&, sig, fmt_view] {
               auto const                flush_fn = [this]( detail::LogEntry const& entry ) { dispatch_engine_.dispatch( entry ); };
               detail::SequentialChunker chunker{ sig, options, sequence_counter_++, flush_fn };
               //
               if constexpr ( sizeof...( Args ) > 0 )
               {
                  std::vformat_to( chunker.iterator( ), fmt_view, std::make_format_args( args... ) ).flush( detail::FlushAction::end );
               }
               else
               {
                  std::ranges::copy( fmt_view, chunker.iterator( ) ).out.flush( detail::FlushAction::end );
               }
            } );
         } };
      }

      auto set_filter( Signal layer, Filter::Op order ) noexcept -> void
      {
         access_policy_.gate( [this, layer, order] { filter_ = { .layer = layer, .order = order }; } );
      }

      [[nodiscard]] auto can_dispatch( Signal sig ) noexcept -> bool
      {
         switch ( auto const [layer, order] = filter_; order )
         {
            case Filter::Op::eq   : return std::equal_to<Signal>{}( sig, layer );
            case Filter::Op::lt   : return std::less<Signal>{}( sig, layer );
            case Filter::Op::lt_eq: return std::less_equal<Signal>{}( sig, layer );
            case Filter::Op::gt   : return std::greater<Signal>{}( sig, layer );
            case Filter::Op::gt_eq: return std::greater_equal<Signal>{}( sig, layer );
            default               : std::unreachable( );
         }
      }
   };
}


#endif //!ASH_LOGGER_HPP
