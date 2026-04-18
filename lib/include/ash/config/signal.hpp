#ifndef ASH_SEVERITY_HPP
#define ASH_SEVERITY_HPP

#include "ash/pch.hpp"


namespace ash
{
   // ───[[ SIGNAL ]]──────────────────────────────────────────────────────────────
   enum class Signal : uint8_t {
      trace = 0U, // Signal for granular execution detail.
      info,       // Signal for general information.
      debug,      // Signal for debugging utility.
      warning,    // Signal for degraded behavior, system still operational.
      error,      // Signal for operation failed, system recovery may be possible.
      fatal       // Signal for catastrophic failure, continuation is UB.
   };

   namespace reflection
   {
      [[nodiscard]] constexpr auto to_string( Signal const sig, bool const uppercase = false ) noexcept -> std::string_view
      {
         switch ( sig )
         {
            case Signal::trace  : return uppercase ? "TRACE" : "trace";
            case Signal::info   : return uppercase ? "INFO" : "info";
            case Signal::debug  : return uppercase ? "DEBUG" : "debug";
            case Signal::warning: return uppercase ? "WARNING" : "warning";
            case Signal::error  : return uppercase ? "ERROR" : "error";
            case Signal::fatal  : return uppercase ? "FATAL" : "fatal";
            default             : std::unreachable( );
         }
      }

      [[nodiscard]] constexpr auto to_string_short( Signal const sig, bool const uppercase = false ) noexcept -> std::string_view
      {
         switch ( sig )
         {
            case Signal::trace  : return uppercase ? "TRC" : "trc";
            case Signal::info   : return uppercase ? "INF" : "inf";
            case Signal::debug  : return uppercase ? "DBG" : "dbg";
            case Signal::warning: return uppercase ? "WRN" : "wrn";
            case Signal::error  : return uppercase ? "ERR" : "err";
            case Signal::fatal  : return uppercase ? "FTL" : "ftl";
            default             : std::unreachable( );
         }
      }
   }
}

// ───[[ SEVERITY FORMATTER ]]──────────────────────────────────────────────────
template <> struct std::formatter<ash::Signal>
{
   constexpr auto parse( std::format_parse_context const& ctx ) noexcept -> std::format_parse_context::const_iterator
   {
      for ( char const* spec = ctx.begin( ); spec != ctx.end( ); ++spec )
      {
         switch ( *spec )
         {
            case 'u': uppercase_ = true; break;
            case 's': repr_ = Repr::shorthand; break;

            default : continue;
            case '}': return spec;
         }
      }
      return ctx.end( );
   }

   constexpr auto format( ash::Signal const sig, std::format_context& ctx ) const noexcept
   {
      return std::format_to( ctx.out( ), "{}", to_repr( sig ) );
   }

private:
   enum class Repr : uint8_t { full, shorthand } repr_ : 7 = Repr::full;
   bool uppercase_ : 1                                     = false;

   [[nodiscard]] constexpr auto to_repr( ash::Signal const sig ) const noexcept -> std::string_view
   {
      switch ( repr_ )
      {
         case Repr::shorthand: return ash::reflection::to_string_short( sig, uppercase_ );
         case Repr::full     : return ash::reflection::to_string( sig, uppercase_ );
         default             : std::unreachable( );
      }
   }
};


#endif //!ASH_SEVERITY_HPP
