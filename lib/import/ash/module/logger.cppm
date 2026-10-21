module;

#include <ash/pch.hpp>

#include <bhop/cbridge.hpp>

export module ash:logger;

import bhop;

import :dispatch;
import :format;
import :policy;
import :signal;
import :time;
import :write_config;


namespace ash::detail
{
   struct SiteHistory
   {
      std::uint32_t hits{ 0 };
      std::uint32_t writes{ 0 };
      std::chrono::steady_clock::time_point last_write{};
   };

   /**
    * Modifiers collected by a \c LogHandle before it writes its entry.
    */
   struct EmitRules
   {
      bool enabled{ true };
      std::uint32_t every{ 1 };
      std::optional<std::uint32_t> cap{};
      std::chrono::nanoseconds throttle{ 0 };

      /**
       * Whether any rule depends on how often the call site was reached, which requires per-site bookkeeping.
       */
      [[nodiscard]] inline auto site_scoped( ) const noexcept -> bool
      {
         return every > 1 || cap.has_value( ) || throttle > std::chrono::nanoseconds::zero( );
      }
   };
}

export namespace ash
{
   /**
    * @brief A pending log entry, written when the handle is destroyed.
    *
    * Returned by the \p Logger signal methods so the entry can be refined before it is written, e.g. `logger.info( "x {}", x ).once( )`. As a
    * temporary, it is destroyed (and the entry written) at the end of the full-expression.
    *
    * Modifiers can be chained and combine with each other:
    *  - `when( condition )` — write only if the condition holds.
    *  - `once( )`, `at_most( n )` — cap how many times this call site writes.
    *  - `every( n )` — write every n-th time this call site is reached.
    *  - `throttle( interval )` — write at most once per interval from this call site.
    * Call-site counters only advance for entries that pass `when` and the logger's signal filter.
    *
    * @warning The handle references its arguments, it does not copy them. If the handle is stored, every argument must outlive it, since the entry
    *          is only written when the handle is destroyed.
    */
   template <typename LoggerT, typename... Args> class LogHandle final
   {
   public:
      LogHandle( LoggerT& logger, Signal signal, FormatAt<std::type_identity_t<Args>...> const& format, Args&&... args ) noexcept
         : logger_{ logger }
         , signal_{ signal }
         , fmt_{ format.fmt }
         , where_{ format.where }
         , args_{ std::forward<Args>( args )... }
      { }

      ~LogHandle( ) noexcept
      {
         std::apply(
           [this]( auto&&... args ) { logger_.submit( signal_, where_, rules_, fmt_, std::forward<decltype( args )>( args )... ); },
           std::move( args_ ) );
      }

      LogHandle( LogHandle const& ) = delete;
      LogHandle( LogHandle&& ) noexcept = delete;
      auto operator=( LogHandle const& ) -> LogHandle& = delete;
      auto operator=( LogHandle&& ) noexcept -> LogHandle& = delete;

      /**
       * Write the entry only if \p condition holds. Several conditions combine as a logical and.
       * @note The arguments are evaluated anyway, since they are evaluated before the call; nothing is formatted or queued though.
       */
      auto when( bool condition ) noexcept -> LogHandle&
      {
         rules_.enabled = rules_.enabled && condition;
         return *this;
      }

      /**
       * Write the entry only the first time this call site is reached.
       */
      auto once( ) noexcept -> LogHandle& { return at_most( 1 ); }

      /**
       * Write the entry at most \p cap times from this call site.
       */
      auto at_most( std::uint32_t cap ) noexcept -> LogHandle&
      {
         rules_.cap = cap;
         return *this;
      }

      /**
       * Write the entry every \p n-th time this call site is reached (the 1st, the n+1-th, ...), e.g. to sample a hot loop.
       */
      auto every( std::uint32_t n ) noexcept -> LogHandle&
      {
         assert( n > 0 && "every( n ) needs n to be at least 1." );
         rules_.every = n;
         return *this;
      }

      /**
       * Write the entry at most once per \p interval from this call site.
       */
      auto throttle( std::chrono::nanoseconds interval ) noexcept -> LogHandle&
      {
         rules_.throttle = interval;
         return *this;
      }

   private:
      LoggerT& logger_;
      Signal const signal_;
      std::format_string<Args...> const fmt_;
      std::source_location const where_;
      detail::EmitRules rules_{};
      std::tuple<Args&&...> args_;
   };

   /**
    * @brief Thread-safe/unsync logger front-end combining an access policy and dispatch policy.
    *
    * Each signal method returns a \p LogHandle, which writes the entry when it is destroyed:
    * @code
    *    logger.info( "frame {}", n );
    *    logger.warning( "missing asset {}", name ).once( );
    *    logger.error( "retrying {}", attempt ).at_most( 3 );
    * @endcode
    *
    * @note Streams referenced by \p WriteConfig must remain valid for the entire lifetime of \p Logger, including any deferred writes still pending
    *       on the worker thread at destruction time.
    *
    * @note Choosing between direct (\p policy::direct_*) and deferred (\p policy::deferred_*) dispatch trades delivery guarantees for caller
    *       latency; see \p policy::Direct and \p policy::Deferred.
    *
    * @tparam D Policy controlling direct vs deferred dispatch.
    * @tparam A Policy controlling sync vs unsync access.
    *
    * Both can be deduced from the constructor arguments, either as a pair or as a \p policy::Bundle preset:
    * @code
    *    ash::Logger a{ ash::policy::deferred, ash::policy::unsync };
    *    ash::Logger b{ ash::policy::deferred_unsync };
    * @endcode
    */
   template <traits::dispatch_policy D, traits::access_policy A> class Logger final
   {
      static_assert( D::retention_policy != policy::RetentionPolicy::milk, "Milk policy is not supported!" );

      template <typename LoggerT, typename... Args> friend class LogHandle;
      template <typename... Args> using handle_type = LogHandle<Logger, Args...>;

   public:
      explicit Logger( schema::WriteConfig config = {} ) noexcept
         : dispatcher_{ std::move( config ) }
      { }

      explicit Logger( policy::Bundle<D, A>, schema::WriteConfig config = {} ) noexcept
         : Logger{ std::move( config ) }
      { }

      explicit Logger( D, A, schema::WriteConfig config = {} ) noexcept
         : Logger{ std::move( config ) }
      { }

      ~Logger( ) = default;

      Logger( Logger const& ) = delete;
      Logger( Logger&& ) noexcept = delete;
      auto operator=( Logger const& ) -> Logger& = delete;
      auto operator=( Logger&& ) noexcept -> Logger& = delete;

      template <typename... Args> auto trace( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::trace, fmt, std::forward<Args>( args )... };
      }

      template <typename... Args> auto info( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::info, fmt, std::forward<Args>( args )... };
      }

      template <typename... Args> auto debug( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::debug, fmt, std::forward<Args>( args )... };
      }

      template <typename... Args> auto warning( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::warning, fmt, std::forward<Args>( args )... };
      }

      template <typename... Args> auto error( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::error, fmt, std::forward<Args>( args )... };
      }

      template <typename... Args> auto fatal( FormatAt<std::type_identity_t<Args>...> const& fmt, Args&&... args ) noexcept -> handle_type<Args...>
      {
         return { *this, Signal::fatal, fmt, std::forward<Args>( args )... };
      }

      auto filter_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::eq ); }
      auto filter_lt( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::lt ); }
      auto filter_lt_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::lt_eq ); }
      auto filter_gt( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::gt ); }
      auto filter_gt_eq( Signal signal ) noexcept -> void { set_filter( signal, Filter::Op::gt_eq ); }
      auto clear_filter( ) noexcept -> void { set_filter( Signal::trace, Filter::Op::gt_eq ); }

   private:
      // Per call-site counters for the site modifiers. Shared by every thread reaching the same call site, so they have their own lock, which is
      // only taken when a site modifier was requested.
      std::unordered_map<std::source_location, detail::SiteHistory, bhop::SourceLocationHash, bhop::SourceLocationCmpEq> history_{};
      A::lock_type history_lock_{};

      Dispatcher<D, A> dispatcher_;

      struct Filter
      {
         Signal signal{ Signal::trace };
         enum class Op : std::uint8_t { eq, lt, lt_eq, gt, gt_eq } order{ Op::gt_eq };
      };
      static_assert( std::atomic<Filter>::is_always_lock_free, "Filter must stay small enough for a lock-free atomic." );

      std::atomic<Filter> filter_{ Filter{} };

      /**
       * Write one entry. Called by \p LogHandle when it is destroyed.
       */
      template <typename... Args>
      auto submit(
        Signal sig, std::source_location const& where, detail::EmitRules const& rules, std::format_string<Args...> fmt, Args&&... args ) noexcept
        -> void
      {
         if ( not rules.enabled || not can_dispatch( sig ) )
         {
            return;
         }
         //
         if ( rules.site_scoped( ) && not admit( where, rules ) )
         {
            return;
         }
         //
         schema::WriteMetadata const metadata{ .sequence_id = 0, .signal = sig, .timestamp = time::TickClock::now( ), .where = where };
         dispatcher_.dispatch( metadata, fmt, std::forward<Args>( args )... );
      }

      /**
       * Record one hit at \p where and decide whether the site rules let it through.
       * @note Keep out-of-line, since this only runs when a site-scoped modifier was requested, and we don't want to bloat the hot path.
       * @return Whether the entry should be written.
       */
      [[nodiscard]] CBR_NO_INLINE auto admit( std::source_location const& where, detail::EmitRules const& rules ) noexcept -> bool
      {
         std::lock_guard lock{ history_lock_ };
         auto& [hits, writes, last_write]{ history_[where] };
         //
         if ( std::uint32_t const hit{ hits++ }; hit % rules.every != 0 )
         {
            return false;
         }
         if ( rules.cap.has_value( ) && writes >= rules.cap.value( ) )
         {
            return false;
         }
         if ( rules.throttle > std::chrono::nanoseconds::zero( ) )
         {
            auto const now = std::chrono::steady_clock::now( );
            if ( writes > 0 && now - last_write < rules.throttle )
            {
               return false;
            }
            last_write = now;
         }
         //
         ++writes;
         return true;
      }

      auto set_filter( Signal signal, Filter::Op order ) noexcept -> void
      {
         filter_.store( { .signal = signal, .order = order }, std::memory_order_relaxed );
      }

      [[nodiscard]] auto can_dispatch( Signal sig ) noexcept -> bool
      {
         switch ( auto const [signal, order] = filter_.load( std::memory_order_relaxed ); order )
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
