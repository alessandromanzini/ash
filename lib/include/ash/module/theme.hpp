#ifndef ASH_THEME_HPP
#define ASH_THEME_HPP

#include "ash/pch.hpp"

#include "ash/module/theme.hpp"


namespace ash::theme
{
   struct Color
   {
      uint8_t r = 0U;
      uint8_t g = 0U;
      uint8_t b = 0U;
      uint8_t a = UINT8_MAX;
   };

   [[nodiscard]] constexpr auto signal_to_color( Signal const sig ) noexcept -> Color
   {
      switch ( sig )
      {
         case Signal::trace   : return { 0xeb, 0xeb, 0xf5, 0x99 }; // muted white, 60% alpha
         case Signal::info    : return { 0x00, 0x7a, 0xff, 0xff }; // system blue
         case Signal::caution : return { 0xff, 0xd6, 0x00, 0xff }; // warning yellow
         case Signal::critical: return { 0xff, 0x9f, 0x0a, 0xff }; // alert orange
         case Signal::fatal   : return { 0xff, 0x45, 0x3a, 0xff }; // danger red
      }
      std::unreachable( );
   }
}


#endif //!ASH_THEME_HPP
