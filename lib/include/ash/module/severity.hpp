#ifndef ASH_SEVERITY_HPP
#define ASH_SEVERITY_HPP

#include "ash/pch.hpp"


namespace ash
{
   // ───[[ SEVERITY ]]────────────────────────────────────────────────────────────
   enum class Severity : uint8_t
   {
      trace,    // verbose diagnostics
      info,     // simple information
      caution,  // something worth watching
      critical, // something went wrong
      fatal     // unrecoverable / hostile
   };

   namespace reflection
   {
      [[nodiscard]] constexpr auto to_string( Severity const sev, bool const uppercase = false ) noexcept -> std::string_view
      {
         switch ( sev )
         {
            case Severity::trace   : return uppercase ? "TRACE" : "trace";
            case Severity::info    : return uppercase ? "INFO" : "info";
            case Severity::caution : return uppercase ? "CAUTION" : "caution";
            case Severity::critical: return uppercase ? "CRITICAL" : "critical";
            case Severity::fatal   : return uppercase ? "FATAL" : "fatal";
            default                : std::unreachable( );
         }
      }

      [[nodiscard]] constexpr auto to_string_short( Severity const sev, bool const uppercase = false ) noexcept
        -> std::string_view
      {
         switch ( sev )
         {
            case Severity::trace   : return uppercase ? "TRC" : "trc";
            case Severity::info    : return uppercase ? "INF" : "inf";
            case Severity::caution : return uppercase ? "CTN" : "ctn";
            case Severity::critical: return uppercase ? "CRT" : "crt";
            case Severity::fatal   : return uppercase ? "FTL" : "ftl";
            default                : std::unreachable( );
         }
      }
   }
}

// ───[[ SEVERITY FORMATTER ]]──────────────────────────────────────────────────
template <> struct std::formatter<ash::Severity>
{
   constexpr auto parse( std::format_parse_context const& ctx ) -> std::format_parse_context::const_iterator
   {
      for ( char const* spec = ctx.begin( ); spec != ctx.end( ); ++spec )
      {
         switch ( *spec )
         {
            case 'u': uppercase_ = true; break;
            case 's': repr_ = Repr::shorthand; break;

            case '}': return spec;
            default : throw std::format_error{ "Invalid ash::Severity format spec." };
         }
      }
      return ctx.end( );
   }

   constexpr auto format( ash::Severity const sev, std::format_context& ctx ) const
   {
      return std::format_to( ctx.out( ), "{}", to_repr( sev ) );
   }

private:
   enum class Repr : uint8_t
   {
      full,
      shorthand
   } repr_ : 7         = Repr::full;
   bool uppercase_ : 1 = false;

   [[nodiscard]] constexpr auto to_repr( ash::Severity const sev ) const noexcept -> std::string_view
   {
      switch ( repr_ )
      {
         case Repr::shorthand: return ash::reflection::to_string_short( sev, uppercase_ );
         case Repr::full     : return ash::reflection::to_string( sev, uppercase_ );
         default             : std::unreachable( );
      }
   }
};


#endif //!ASH_SEVERITY_HPP
