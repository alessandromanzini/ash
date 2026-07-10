#ifndef ASH_SIGNAL_HPP
#define ASH_SIGNAL_HPP

#include <ash/pch.hpp>

#include <ash/module/format_flags.hpp>


namespace ash
{
   /**
    * \p Signal is an enumeration that represents degree of importance of the data it is linked to.
    */
   enum class Signal : uint8_t {
      trace = 0, // For granular execution detail.
      debug,     // For debugging utility.
      info,      // For general information.
      warning,   // For degraded behavior, system still operational.
      error,     // For operation failed, system recovery may be possible.
      fatal      // For catastrophic failure, continuation is UB.
   };

   namespace reflection
   {
      [[nodiscard]] constexpr auto to_string( Signal sig, FormatFlags const& spec = {} ) noexcept -> std::string_view
      {
         switch ( spec.representation )
         {
            case FormatFlags::Repr::full:
               switch ( sig )
               {
                  case Signal::trace  : return spec.uppercase ? "TRACE" : "trace";
                  case Signal::debug  : return spec.uppercase ? "DEBUG" : "debug";
                  case Signal::info   : return spec.uppercase ? "INFO" : "info";
                  case Signal::warning: return spec.uppercase ? "WARNING" : "warning";
                  case Signal::error  : return spec.uppercase ? "ERROR" : "error";
                  case Signal::fatal  : return spec.uppercase ? "FATAL" : "fatal";
               }
               break;

            case FormatFlags::Repr::shorthand:
               switch ( sig )
               {
                  case Signal::trace  : return spec.uppercase ? "TRC" : "trc";
                  case Signal::debug  : return spec.uppercase ? "DBG" : "dbg";
                  case Signal::info   : return spec.uppercase ? "INF" : "inf";
                  case Signal::warning: return spec.uppercase ? "WRN" : "wrn";
                  case Signal::error  : return spec.uppercase ? "ERR" : "err";
                  case Signal::fatal  : return spec.uppercase ? "FTL" : "ftl";
               }
               break;
         }
         std::unreachable( );
      }
   }
}


#endif //!ASH_SIGNAL_HPP
