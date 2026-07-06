module;

#include <cstdint> // global uint8_t/uint32_t

export module ash:logger;

import std;

import :signal;
import :policy;
import :writer;


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
      explicit Logger( AccessPolicy access, DispatchPolicy dispatch, cfg::LogConfig config = {} ) noexcept
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
            // In case of a deferred dispatch, we must guarantee the SPSC CONTRACT ...
            // A. UNSYNC access policy: we only ever access the engine from the same thread, no issues here.
            // B. SYNC access policy: the gate mutex ensures the contract.
            access_policy_.gate( [&, sig, fmt_view] {
               auto const flush_fn = [this]( Message const& message ) { dispatch_engine_.dispatch( message ); };
               detail::SequentialChunker chunker{ sig, options, sequence_counter_++, flush_fn };
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
