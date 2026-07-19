module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:logger;

import :dispatch;
import :policy;
import :signal;
import :write_config;


namespace ash::detail
{
   /**
    * Key equality for \c std::source_location structure. The file name is compared last for faster evaluation. When the line and column match, it
    * performs a final character of the file name, short-circuiting in case of the same TU case where the c-string address would be the same, falling
    * back to character-per-character test.
    */
   struct SourceLocationCmpEq
   {
      [[nodiscard]] auto operator( )( std::source_location const& lhs, std::source_location const& rhs ) const noexcept -> bool
      {
         return lhs.line( ) == rhs.line( ) && lhs.column( ) == rhs.column( ) &&
                ( lhs.file_name( ) == rhs.file_name( ) || std::string_view{ lhs.file_name( ) } == rhs.file_name( ) );
      }
   };
}

/**
 * Hash for \c std::source_location, keyed on the call site's line/column pair.
 * @note The file name is deliberately left out of the hash since its pointer is not stable for the same header included in different TUs, which means
 *       each hash would cost a full scan.
 */
template <> struct std::hash<std::source_location>
{
   [[nodiscard]] auto operator( )( std::source_location const& loc ) const noexcept -> size_t
   {
      // Fibonacci (golden ratio) multiplicative hash over the packed line/column pair.
      uint64_t const packed = static_cast<uint64_t>( loc.line( ) ) << 32 | loc.column( );
      return static_cast<size_t>( packed * 0x9E37'79B9'7F4A'7C15ULL );
   }
};

export namespace ash
{
   template <typename DispatchFn> class LoggerOperator final // TODO: reimagine access.
   {
   public:
      LoggerOperator( DispatchFn dispatch_fn ) noexcept
         : dispatch_fn_{ dispatch_fn }
      { }

      template <typename... Args> auto trace( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::trace, fmt, std::forward<Args>( args )... );
      }

      auto trace( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::trace, view ); }

      template <typename... Args> auto info( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::info, fmt, std::forward<Args>( args )... );
      }

      auto info( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::info, view ); }

      template <typename... Args> auto debug( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::debug, fmt, std::forward<Args>( args )... );
      }

      auto debug( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::debug, view ); }

      template <typename... Args> auto warning( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::warning, fmt, std::forward<Args>( args )... );
      }

      auto warning( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::warning, view ); }

      template <typename... Args> auto error( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::error, fmt, std::forward<Args>( args )... );
      }

      auto error( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::error, view ); }

      template <typename... Args> auto fatal( std::format_string<Args...> fmt, Args&&... args ) noexcept -> void
      {
         std::invoke( dispatch_fn_, Signal::fatal, fmt, std::forward<Args>( args )... );
      }

      auto fatal( std::string_view view ) noexcept -> void { std::invoke( dispatch_fn_, Signal::fatal, view ); }

   private:
      DispatchFn const dispatch_fn_;
   };

   /**
    * @brief Thread-safe/unsync logger front-end combining an access policy and dispatch policy.
    *
    * @note Streams referenced by \p WriteConfig must remain valid for the entire lifetime of \p Logger, including any deferred writes still pending
    *       on the worker thread at destruction time.
    *
    * @note Deferred dispatch becomes mostly valuable when titles and bodies are difficult to process (long and/or format-heavy). The price of worker
    *       thread synchronization might be overkill if that prior condition is not met.
    *
    * @tparam AccessPolicy   Policy controlling synch vs unsynch access.
    * @tparam DispatchPolicy Policy controlling direct vs deferred dispatch.
    */
   template <traits::access_policy AccessPolicy, traits::dispatch_policy DispatchPolicy> class Logger final
   {
      static_assert( DispatchPolicy::retention_policy != policy::RetentionPolicy::milk, "Milk policy is not supported!" );

   public:
      explicit Logger( AccessPolicy access, DispatchPolicy dispatch, schema::WriteConfig config = {} ) noexcept
         : access_policy_{ std::move( access ) }
         , dispatch_policy_{ std::move( dispatch ) }
         , dispatcher_{ std::move( config ) }
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

      std::unordered_map<std::source_location, uint32_t, std::hash<std::source_location>, detail::SourceLocationCmpEq> echo_counts_{};

      detail::Dispatcher<DispatchPolicy> dispatcher_;
      schema::WriteMetadata::sequence_id_type sequence_id_ = 0;

      struct Filter
      {
         Signal signal = Signal::trace;
         enum class Op : uint8_t { eq, lt, lt_eq, gt, gt_eq } order = Op::gt_eq;
      } filter_{};

      auto make_operator( schema::WriteOptions const& options ) noexcept
      {
         // Fmt is either a std::format_string<Args...> (format overloads) or a std::string_view (pre-rendered overloads).
         return LoggerOperator{ [this, options]<typename Fmt, typename... Args>( Signal sig, Fmt fmt, Args&&... args ) {
            if ( not can_dispatch( sig ) )
            {
               return;
            }
            //
            access_policy_.gate( [&, sig, fmt] {
               // Consume the echo inside the gate so the count mutation is covered by the access policy. std::source_location does not have
               // collisions, but the same location can be accessed by different threads, therefore we need to sync.
               if ( options.echo.has_value( ) && not consume_echo( options ) )
               {
                  return;
               }
               //
               // In case of a deferred dispatcher, we must guarantee the SPSC CONTRACT ...
               // A. UNSYNC access policy: we only ever access the engine from the same thread, no issues here.
               // B. SYNC access policy: the gate mutex ensures the contract.
               schema::WriteMetadata const metadata{
                  .sequence_id = sequence_id_++, .signal = sig, .identity = "TEST", .timestamp = std::chrono::system_clock::now( ), .options = options
               };
               dispatcher_.dispatch( metadata, fmt, std::forward<Args>( args )... );
            } );
         } };
      }

      /**
       * Burn one echo charge at the \p std::source_location hash.
       * @note Keep out-of-line, since this is a conditional op depending on the echo request, and we don't want to bloat the hot path.
       * @return Whether the request did not exceed the echo counter.
       */
      [[nodiscard]] CBR_NO_INLINE auto consume_echo( schema::WriteOptions const& options ) noexcept -> bool
      {
         uint32_t& used = echo_counts_[options.where];
         if ( used >= options.echo.value( ) )
         {
            return false;
         }
         ++used;
         return true;
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
