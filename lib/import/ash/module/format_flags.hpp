#ifndef ASH_FORMAT_FLAGS_HPP
#define ASH_FORMAT_FLAGS_HPP

#include <ash/pch.hpp>


namespace ash
{
   struct FormatFlags
   {
      enum class Repr : uint8_t { full, shorthand } representation : 7 = Repr::full;
      bool uppercase : 1 = false;
   };
}


#endif //!ASH_FORMAT_FLAGS_HPP
