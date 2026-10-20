#include <ash/__internal/clock.hpp>

#ifndef WIN32_LEAN_AND_MEAN
   #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
   #define NOMINMAX
#endif
#include <windows.h>


namespace ash::time
{
   auto TickClock::now( ) noexcept -> repr
   {
      LARGE_INTEGER ticks;
      QueryPerformanceCounter( &ticks );
      return static_cast<repr>( ticks.QuadPart );
   }

   auto TickClock::frequency( ) noexcept -> repr
   {
      static repr const hz{ [] {
         LARGE_INTEGER frequency;
         QueryPerformanceFrequency( &frequency );
         return static_cast<repr>( frequency.QuadPart );
      }( ) };
      return hz;
   }
}
