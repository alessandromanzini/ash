#ifndef ASH_SIGNAL_HPP
#define ASH_SIGNAL_HPP

#include <cstdint>
#include <string_view>
#include <utility>


namespace ash
{
   // ───[[ SIGNAL ]]──────────────────────────────────────────────────────────────
   enum class Signal : uint8_t {
      trace = 0, // Signal for granular execution detail.
      debug,     // Signal for debugging utility.
      info,      // Signal for general information.
      warning,   // Signal for degraded behavior, system still operational.
      error,     // Signal for operation failed, system recovery may be possible.
      fatal      // Signal for catastrophic failure, continuation is UB.
   };

   namespace reflection
   {
      [[nodiscard]] constexpr auto to_string( Signal sig, bool uppercase = false ) noexcept -> std::string_view
      {
         switch ( sig )
         {
            case Signal::trace  : return uppercase ? "TRACE" : "trace";
            case Signal::debug  : return uppercase ? "DEBUG" : "debug";
            case Signal::info   : return uppercase ? "INFO" : "info";
            case Signal::warning: return uppercase ? "WARNING" : "warning";
            case Signal::error  : return uppercase ? "ERROR" : "error";
            case Signal::fatal  : return uppercase ? "FATAL" : "fatal";
            default             : std::unreachable( );
         }
      }

      [[nodiscard]] constexpr auto to_short_string( Signal sig, bool uppercase = false ) noexcept -> std::string_view
      {
         switch ( sig )
         {
            case Signal::trace  : return uppercase ? "TRC" : "trc";
            case Signal::debug  : return uppercase ? "DBG" : "dbg";
            case Signal::info   : return uppercase ? "INF" : "inf";
            case Signal::warning: return uppercase ? "WRN" : "wrn";
            case Signal::error  : return uppercase ? "ERR" : "err";
            case Signal::fatal  : return uppercase ? "FTL" : "ftl";
            default             : std::unreachable( );
         }
      }
   }
}


#endif //!ASH_SIGNAL_HPP
