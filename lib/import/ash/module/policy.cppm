module;

#include <ash/pch.hpp>

export module ash:policy;


/**
 * Access policies, that control how the consumer threads interact.
 */
export namespace ash::policy
{
   struct Sync
   {
      struct access_policy_tag
      { };

      mutable std::mutex mutex{};

      Sync( ) noexcept = default;
      Sync( Sync const& ) noexcept { }
      Sync( Sync&& ) noexcept { }

      template <std::invocable F> auto gate( F func ) const noexcept( std::is_nothrow_invocable_v<F> ) -> decltype( func( ) )
      {
         std::lock_guard lock{ mutex };
         return func( );
      }
   } inline const sync;

   struct Unsync
   {
      struct access_policy_tag
      { };

      template <std::invocable F> static auto gate( F func ) noexcept( std::is_nothrow_invocable_v<F> ) -> decltype( func( ) ) { return func( ); }
   } inline constexpr unsync;
}

/**
 * Dispatch policies, that control where information is flushed, how and how much data may be retained.
 */
export namespace ash::policy
{
   enum class DispatchTiming : std::uint8_t { direct, deferred };
   enum class RetentionPolicy : std::uint8_t { milk /* Discard oldest. */, wine /* Discard newest. */ };
   enum class OverflowPolicy : std::uint8_t { truncate, send_to_heap };

   /**
    * @brief Formats and writes on the calling thread, before the log call returns.
    *
    * Prefer it when every entry must reach the sink:
    *  - Nothing is ever dropped; a slow sink slows the caller down instead.
    *  - Entries are written before a crash or abort, which makes it the safe choice for error and fatal paths.
    *  - Output stays ordered with any other writes to the same streams, and is visible immediately while debugging.
    *  - No worker thread, no ring buffer, and any type with a \c std::formatter can be logged (no \c traits::Codec needed).
    *  - Cheaper overall when logging is sparse, cores are scarce, or many loggers coexist.
    */
   struct Direct
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::direct;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr std::size_t pool_size{ 0 };
      static constexpr std::size_t inline_buffer_size{ 512 };
      static constexpr auto overflow_policy = OverflowPolicy::truncate;
   } inline constexpr direct;

   /**
    * @brief Copies the arguments into a ring buffer on the calling thread; a worker thread formats and writes them later.
    *
    * Prefer it when the caller's latency matters more than delivery, e.g. a game loop or a request handler logging in bursts: the caller only pays
    * for the argument copy, and the formatting cost moves to the worker.
    *
    * @warning Entries are dropped without notice once the ring is full ('wine' retention), so sustained logging is capped by the worker's
    *          throughput. Entries still queued are lost if the process crashes or aborts.
    * @note Entries can be written after unrelated output that happened later on the same streams.
    * @note Every argument type needs a \c traits::Codec specialization.
    * @note When logging is sparse the worker sleeps between entries, and each entry pays for waking it up.
    */
   struct Deferred
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::deferred;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr std::size_t pool_size{ 64 };
      static constexpr std::size_t inline_buffer_size{ 0 }; // Setting this value to 0 defaults to the optimal Message size.
      static constexpr auto overflow_policy = OverflowPolicy::truncate;
   } inline constexpr deferred;
}

/**
 * Concept traits for policies.
 */
namespace ash::traits::detail
{
   template <typename P>
   concept policy = std::is_nothrow_constructible_v<P> && std::is_nothrow_move_constructible_v<P> && std::is_nothrow_destructible_v<P>;
}

export namespace ash::traits
{
   template <typename P>
   concept access_policy = detail::policy<P> && requires { typename P::access_policy_tag; } && requires( P policy ) {
      {
         policy.gate( []( ) -> int { return 0; } )
      } -> std::same_as<int>;
   };

   template <typename P>
   concept dispatch_policy = detail::policy<P> && requires { typename P::dispatch_policy_tag; } && requires {
      { P::dispatch_timing } -> std::convertible_to<policy::DispatchTiming>;
      { P::retention_policy } -> std::convertible_to<policy::RetentionPolicy>;
      { P::overflow_policy } -> std::convertible_to<policy::OverflowPolicy>;
      { P::pool_size } -> std::convertible_to<std::size_t>;
      { P::inline_buffer_size } -> std::convertible_to<std::size_t>;
   };
}
