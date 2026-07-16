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
   enum class DispatchTiming : uint8_t { direct, deferred };
   enum class RetentionPolicy : uint8_t { milk /* Discard oldest. */, wine /* Discard newest. */ };
   enum class OverflowPolicy : uint8_t { truncate, send_to_heap };

   struct Direct
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::direct;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr size_t pool_size = 0;
      static constexpr size_t inline_buffer_size = 512;
      static constexpr auto overflow_policy = OverflowPolicy::truncate;
   } inline constexpr direct;

   struct Deferred
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::deferred;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr size_t pool_size = 64;
      static constexpr size_t inline_buffer_size = 204;
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
      { P::pool_size } -> std::convertible_to<size_t>;
      { P::inline_buffer_size } -> std::convertible_to<size_t>;
   };
}
