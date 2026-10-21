#include <ash/__internal/clock.hpp>

#include <bhop/config/config.hpp>
#include <mach/mach_time.h>


namespace ash::time
{
   auto TickClock::now( ) noexcept -> repr
   {
      if constexpr ( bhop::cfg::arch_type == bhop::cfg::ArchType::arm64 )
      {
         repr ticks;
         __asm__ volatile( "mrs %0, cntvct_el0" : "=r"( ticks ) );
         return ticks;
      }
      else
      {
         return mach_absolute_time( );
      }
   }

   auto TickClock::frequency( ) noexcept -> repr
   {
      if constexpr ( bhop::cfg::arch_type == bhop::cfg::ArchType::arm64 )
      {
         repr hz;
         __asm__ volatile( "mrs %0, cntfrq_el0" : "=r"( hz ) );
         return hz;
      }
      else
      {
         // The timebase converts ticks to nanoseconds as ticks * numer / denom.
         static repr const hz{ [] {
            mach_timebase_info_data_t timebase{};
            mach_timebase_info( &timebase );
            return repr{ 1'000'000'000 } * timebase.denom / timebase.numer;
         }( ) };
         return hz;
      }
   }
}
