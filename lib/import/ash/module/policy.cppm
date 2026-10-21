module;

#include <ash/pch.hpp>

export module ash:policy;

import bhop;


export namespace ash::policy
{
   enum class DispatchTiming : std::uint8_t { direct, deferred };
   enum class RetentionPolicy : std::uint8_t { milk /* Discard oldest. */, wine /* Discard newest. */ };
   enum class OverflowPolicy : std::uint8_t { truncate, send_to_heap };
}

/**
 * Dispatch policies, that control where information is flushed, how and how much data may be retained.
 */
export namespace ash::policy
{
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
    *
    * @note Every argument type needs a \c traits::Codec specialization.
    * @note Setting \c inline_buffer_size to \p 0 defaults to the optimal pool \c Message size.
    * @note The caller never wakes the worker: after a short burst of yielding, an idle worker sleeps \p poll_interval between checks of the ring.
    */
   template <std::size_t _poll_size, bool _ordered> struct BasicDeferred
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::deferred;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr auto pool_size = _poll_size;
      static constexpr std::size_t inline_buffer_size{ 0 };
      static constexpr auto overflow_policy = OverflowPolicy::truncate;
      static constexpr std::chrono::microseconds poll_interval{ 100 };
      static constexpr auto ordered = _ordered;
   };

   /**
    * A \c pool_size of 256 will allocate 64KB on the heap, which absorbs bursts of ~2.5M entries/s for one poll interval while staying mostly
    * cache-resident. Larger rings absorb more but cost the caller latency (the slots it writes are no longer in L1).
    */
   struct Deferred : BasicDeferred<256, false>
   {
   } inline constexpr deferred{};

   /**
    * Considering thread_local approaches would fail in an ordered context, we need shared resources. For this reason we default the \p pool_size to
    * 4x the unordered default size.
    */
   struct DeferredOrdered : BasicDeferred<1'024, true>
   {
   } inline constexpr deferred_ordered{};
}

/**
 * Access policies, that control how the consumer threads interact.
 */
export namespace ash::policy
{
   /**
    * @brief The logger may be called from several threads at once.
    */
   struct Sync
   {
      struct access_policy_tag
      { };

      using lock_type = std::mutex;
      static constexpr bool multi_producer{ true };
   } inline constexpr sync;

   /**
    * @brief The logger is only ever called from one thread at a time; no locking at all.
    */
   struct Unsync
   {
      struct access_policy_tag
      { };

      using lock_type = bhop::VoidLock;
      static constexpr bool multi_producer{ false };
   } inline constexpr unsync;
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
   concept dispatch_policy = detail::policy<P> && requires {
      { P::dispatch_timing } -> std::convertible_to<policy::DispatchTiming>;
      { P::retention_policy } -> std::convertible_to<policy::RetentionPolicy>;
      { P::overflow_policy } -> std::convertible_to<policy::OverflowPolicy>;
      { P::inline_buffer_size } -> std::convertible_to<std::size_t>;

      requires P::dispatch_timing != policy::DispatchTiming::deferred || requires {
         { P::pool_size } -> std::convertible_to<std::size_t>;
         { P::poll_interval } -> std::convertible_to<std::chrono::microseconds>;
         { P::ordered } -> std::convertible_to<bool>;
      };
   };

   template <typename P>
   concept access_policy = detail::policy<P> && requires {
      typename P::access_policy_tag;
      typename P::lock_type;
      { P::multi_producer } -> std::convertible_to<bool>;
   } && bhop::traits::basic_lockable<typename P::lock_type>;
}

export namespace ash::policy
{
   /**
    * This is a bundle to create policy presets that act as shortcuts when passing traits to a class.
    * @tparam D
    * @tparam A
    */
   template <traits::dispatch_policy D, traits::access_policy A> struct Bundle
   {
      using dispatch_type = D;
      using access_type = A;

      Bundle( ) noexcept = default;
      consteval explicit Bundle( D, A ) noexcept { }
   };

   inline constexpr auto direct_sync = Bundle{ direct, sync };
   inline constexpr auto direct_unsync = Bundle{ direct, unsync };
   inline constexpr auto deferred_sync = Bundle{ deferred, sync };
   inline constexpr auto deferred_ordered_sync = Bundle{ deferred_ordered, sync };
   inline constexpr auto deferred_unsync = Bundle{ deferred, unsync };
}
