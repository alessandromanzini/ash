#ifndef SWIFTLOG_LIBRARY_HPP
#define SWIFTLOG_LIBRARY_HPP

#include "ash/pch.hpp"


namespace ash
{
   ASH_EXPORT void log(std::source_location source_location = std::source_location::current());

   // class LogBuilder
   // {
   //    std::string_view;
   //    int count;
   // };
   // class SWIFTGUARD_EXPORT Guard final
   // {
   // public:
   //    Guard() = default;
   //
   //    assert();
   //    log();
   //
   // private:
   // };
}


#endif //!SWIFTLOG_LIBRARY_HPP
