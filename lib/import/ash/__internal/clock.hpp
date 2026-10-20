#ifndef ASH_CLOCK_HPP
#define ASH_CLOCK_HPP

#include <ash/pch.hpp>


namespace ash::time
{
   /**
    * @brief Raw monotonic tick counter, read as cheaply as the platform allows so stamping an entry costs next to nothing on the caller.
    *
    * Ticks only become wall-clock time through a \p TickAnchor, which is meant to run on the consumer side (e.g. the deferred worker).
    *
    * Sources:
    *  - Darwin arm64:  the virtual counter register (cntvct_el0), frequency from cntfrq_el0.
    *  - Darwin x86_64: mach_absolute_time, frequency from the mach timebase.
    *  - Win32:         QueryPerformanceCounter / QueryPerformanceFrequency.
    */
   struct TickClock
   {
      using repr = std::uint64_t;

      [[nodiscard]] ASH_API static auto now( ) noexcept -> repr;
      // { standard lib version (slow)
      //    return static_cast<repr>( std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) );
      // }

      /**
       * @return Ticks per second.
       */
      [[nodiscard]] ASH_API static auto frequency( ) noexcept -> repr;
      // { standard lib version (slow)
      //    using period = std::chrono::steady_clock::period;
      //    return static_cast<repr>( period::den / period::num );
      // }
   };

   /**
    * @brief Maps ticks to wall-clock time, from one (ticks, system_clock) pair taken at construction.
    *
    * @note Re-anchor regularly (see \c is_stale): tick counters can pause while the machine sleeps, and the wall clock can be adjusted.
    */
   class TickAnchor final
   {
   public:
      TickAnchor( ) noexcept
         : ticks_{ TickClock::now( ) }
         , wall_{ std::chrono::system_clock::now( ) }
         , frequency_{ static_cast<std::int64_t>( TickClock::frequency( ) ) }
      { }

      [[nodiscard]] auto to_system( TickClock::repr ticks ) const noexcept -> std::chrono::system_clock::time_point
      {
         // Signed, since entries queued before the anchor was taken map to an earlier time. Split into whole seconds and remainder so the
         // nanosecond conversion cannot overflow.
         std::int64_t const delta{ static_cast<std::int64_t>( ticks - ticks_ ) };
         std::int64_t const nanoseconds{ delta / frequency_ * 1'000'000'000 + delta % frequency_ * 1'000'000'000 / frequency_ };
         return wall_ + std::chrono::duration_cast<std::chrono::system_clock::duration>( std::chrono::nanoseconds{ nanoseconds } );
      }

      /**
       * @return Whether \p ticks is more than a second past the anchor, at which point it should be re-taken.
       */
      [[nodiscard]] auto is_stale( TickClock::repr ticks ) const noexcept -> bool
      {
         return static_cast<std::int64_t>( ticks - ticks_ ) > frequency_;
      }

   private:
      TickClock::repr ticks_;
      std::chrono::system_clock::time_point wall_;
      std::int64_t frequency_;
   };
}


#endif //!ASH_TICK_CLOCK_HPP
