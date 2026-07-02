module;

#include <ash/pch.hpp>

#include <ash/config/signal.hpp>

export module ash:signal;


export namespace ash
{
   using ash::Signal;

   namespace reflection
   {
      using reflection::to_short_string;
      using reflection::to_string;
   }
}

// ───[[ SIGNAL FORMATTER ]]────────────────────────────────────────────────────
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

   constexpr auto format( ash::Signal sig, std::format_context& ctx ) const noexcept // NOLINT(*-exception-escape)
   {
      return std::format_to( ctx.out( ), "{}", to_repr( sig ) );
   }

private:
   enum class Repr : uint8_t { full, shorthand } repr_ : 7 = Repr::full;
   bool uppercase_ : 1 = false;

   [[nodiscard]] constexpr auto to_repr( ash::Signal sig ) const noexcept -> std::string_view
   {
      switch ( repr_ )
      {
         case Repr::shorthand: return ash::reflection::to_short_string( sig, uppercase_ );
         case Repr::full     : return ash::reflection::to_string( sig, uppercase_ );
         default             : std::unreachable( );
      }
   }
};
