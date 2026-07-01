#ifndef ASH_POLICY_HPP
#define ASH_POLICY_HPP

#include <ash/pch.hpp>


// ───[[ ACCESS ]]────────────────────────────────────────────────────────────
namespace ash::policy
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

// ───[[ DISPATCH ]]────────────────────────────────────────────────────────────
namespace ash::policy
{
   enum class DispatchTiming : uint8_t { direct, deferred };
   enum class RetentionPolicy : uint8_t { milk /* Discard oldest. */, wine /* Discard newest. */ };

   struct Direct
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::direct;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr size_t pool_size = 0;
   } inline constexpr direct;

   struct Deferred
   {
      struct dispatch_policy_tag
      { };

      static constexpr auto dispatch_timing = DispatchTiming::deferred;
      static constexpr auto retention_policy = RetentionPolicy::wine;
      static constexpr size_t pool_size = 64;
   } inline constexpr deferred;
}


namespace ash::traits::detail
{
   template <typename P>
   concept policy = std::is_nothrow_constructible_v<P> && std::is_nothrow_move_constructible_v<P> && std::is_nothrow_destructible_v<P>;
}

namespace ash::traits
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
      { P::pool_size } -> std::convertible_to<size_t>;
   };
}


#endif //!ASH_POLICY
