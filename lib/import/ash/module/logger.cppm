module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:logger;

import :signal;
import :policy;
import :writer;

namespace ash::detail
{
   /**
    * This class flushes strings in chunks through its iterator. It is mostly useful for applications where the string size is not known at
    * compile-time, allowing the representation of any sentence without the heap allocations and resizes of traditional strings.
    *
    * This class was specifically designed for string formatting using the standard library function std::format_to which writes directly to the
    * chunker iterator and flushes to the output stream.
    */
   class StringChunker final
   {
      class Iterator final
      {
      public:
         using iterator_category = std::output_iterator_tag;
         using value_type = void;
         using difference_type = std::ptrdiff_t;
         using pointer = void;
         using reference = void;

         CBR_FORCE_INLINE explicit Iterator( StringChunker& chunker, Message& message )
            : chunker_ptr_{ &chunker }
            , message_ptr_{ &message }
         { }

         CBR_FORCE_INLINE auto operator*( ) noexcept -> Iterator& { return *this; }
         CBR_FORCE_INLINE auto operator++( ) noexcept -> Iterator& { return *this; }
         CBR_FORCE_INLINE auto operator++( int ) const noexcept -> Iterator { return *this; }

         CBR_FORCE_INLINE auto operator=( char c ) noexcept -> Iterator&
         {
            Message::chunk_size_type& buffer_pos = message_ptr_->chunk_len;
            if ( constexpr size_t max_msg_size = sizeof( Message::content ) - 1ULL; buffer_pos == max_msg_size )
            {
               flush( cfg::FlushAction::carry_on );
            }
            message_ptr_->content[buffer_pos++] = c;
            //
            return *this;
         }

         CBR_FORCE_INLINE auto flush( cfg::FlushAction action ) const noexcept -> void
         {
            message_ptr_->chunk_position = [this, action] -> Message::ChunkPosition {
               bool const is_first = message_ptr_->chunk_id == 0;
               bool const is_last = action == cfg::FlushAction::end;
               if ( is_first && is_last )
               {
                  return Message::ChunkPosition::sole;
               }
               if ( is_first )
               {
                  return Message::ChunkPosition::head;
               }
               if ( is_last )
               {
                  return Message::ChunkPosition::tail;
               }
               return Message::ChunkPosition::body;
            }( );
            //
            message_ptr_->chunk_id++;
            message_ptr_->content[message_ptr_->chunk_len++] = '\0';
            //
            std::invoke( chunker_ptr_->flush_fn_, *message_ptr_ );
            message_ptr_->chunk_len = 0;
         }

      private:
         StringChunker* chunker_ptr_ = nullptr;
         Message* message_ptr_ = nullptr;
      };

   public:
      using flush_fn_type = std::function<void( Message const& )>; // This should be std::move_only_function when clang supports it :)

      CBR_FORCE_INLINE explicit StringChunker(
        Signal sig, WriteOptions const& options, Message::sequence_id_type seq_id, flush_fn_type flush_fn ) noexcept
         : flush_fn_{ std::move( flush_fn ) }
         , current_chunk_{ .timestamp = std::time( nullptr ),
                           .options = options,
                           .signal = sig,
                           .chunk_position = Message::ChunkPosition::head,
                           .chunk_id = 0,
                           .sequence_id = seq_id,
                           .chunk_len = 0,
                           .content = {} }
      { }

      CBR_FORCE_INLINE auto iterator( ) noexcept -> Iterator { return Iterator{ *this, current_chunk_ }; }

   private:
      flush_fn_type const flush_fn_{};
      Message current_chunk_{};
   };
}

export namespace ash
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
    * @note Streams referenced by \p LogConfig must remain valid for the entire lifetime of \p Logger, including any deferred writes still pending
    *       on the worker thread at destruction time.
    *
    * @tparam AccessPolicy   Policy controlling synch vs unsynch access.
    * @tparam DispatchPolicy Policy controlling direct vs deferred dispatch.
    */
   template <traits::access_policy AccessPolicy, traits::dispatch_policy DispatchPolicy> class Logger final
   {
      static_assert( DispatchPolicy::retention_policy != policy::RetentionPolicy::milk, "Milk policy is not supported!" );

   public:
      explicit Logger( AccessPolicy access, DispatchPolicy dispatch, cfg::WriteConfig config = {} ) noexcept
         : access_policy_{ std::move( access ) }
         , dispatch_policy_{ std::move( dispatch ) }
         , dispatch_engine_{ std::move( config ) }
      { }
      ~Logger( ) = default;

      Logger( Logger const& ) = delete;
      Logger( Logger&& ) noexcept = delete;
      auto operator=( Logger const& ) -> Logger& = delete;
      auto operator=( Logger&& ) noexcept -> Logger& = delete;

      auto emit( std::source_location loc = std::source_location::current( ) ) noexcept
      {
         return make_operator( { .echo = std::nullopt, .where = loc } );
      }

      auto once( std::source_location loc = std::source_location::current( ) ) noexcept { return make_operator( { .echo = 1, .where = loc } ); }

      auto at_most( uint32_t cap, std::source_location loc = std::source_location::current( ) ) noexcept
      {
         return make_operator( { .echo = cap, .where = loc } );
      }

      auto filter_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::eq ); }
      auto filter_lt( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::lt ); }
      auto filter_lt_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::lt_eq ); }
      auto filter_gt( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::gt ); }
      auto filter_gt_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::gt_eq ); }

      auto clear_filter( ) noexcept -> void { set_filter( Signal::trace, Filter::Op::gt_eq ); }

   private:
      [[no_unique_address]] AccessPolicy access_policy_;
      [[no_unique_address]] DispatchPolicy dispatch_policy_;

      DispatchEngine<DispatchPolicy> dispatch_engine_;
      uint32_t sequence_counter_{ 0U };

      struct Filter
      {
         Signal signal = Signal::trace;
         enum class Op : uint8_t { eq, lt, lt_eq, gt, gt_eq } order = Op::gt_eq;
      } filter_{};

      auto make_operator( WriteOptions const& options ) noexcept
      {
         return LoggerOperator{ [this, options]<typename... Args>( Signal sig, std::string_view fmt_view, Args&&... args ) {
            if ( not can_dispatch( sig ) )
            {
               return;
            }
            //
            access_policy_.gate( [&, sig, fmt_view] {
               // In case of a deferred dispatch, we must guarantee the SPSC CONTRACT ...
               // A. UNSYNC access policy: we only ever access the engine from the same thread, no issues here.
               // B. SYNC access policy: the gate mutex ensures the contract.
               auto const flush_fn = [this]( Message const& message ) { dispatch_engine_.dispatch( message ); };
               detail::StringChunker chunker{ sig, options, sequence_counter_++, flush_fn };
               //
               if constexpr ( sizeof...( Args ) > 0 )
               {
                  std::vformat_to( chunker.iterator( ), fmt_view, std::make_format_args( args... ) ).flush( cfg::FlushAction::end );
               }
               else
               {
                  std::ranges::copy( fmt_view, chunker.iterator( ) ).out.flush( cfg::FlushAction::end );
               }
            } );
         } };
      }

      auto set_filter( Signal signal, Filter::Op order ) noexcept -> void
      {
         access_policy_.gate( [this, signal, order] { filter_ = { .signal = signal, .order = order }; } );
      }

      [[nodiscard]] auto can_dispatch( Signal sig ) noexcept -> bool
      {
         switch ( auto const [signal, order] = filter_; order )
         {
            case Filter::Op::eq   : return sig == signal;
            case Filter::Op::lt   : return sig < signal;
            case Filter::Op::lt_eq: return sig <= signal;
            case Filter::Op::gt   : return sig > signal;
            case Filter::Op::gt_eq: return sig >= signal;
            default               : std::unreachable( );
         }
      }
   };
}
