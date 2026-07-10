module;

#include <ash/pch.hpp>

#include <ash/module/format_flags.hpp>
#include <ash/module/signal.hpp>

export module ash:signal;


export namespace ash
{
   using ash::Signal;

   namespace reflection
   {
      using reflection::to_string;
   }
}

/**
 * Custom formatter for the \p Signal enumerator.
 * @note Also provides :u (uppercase) and :s (short) representations.
 */
export template <> struct std::formatter<ash::Signal>
{
   constexpr auto parse( std::format_parse_context const& ctx ) noexcept -> std::format_parse_context::const_iterator
   {
      for ( char const* spec = ctx.begin( ); spec != ctx.end( ); ++spec )
      {
         switch ( *spec )
         {
            case 'u': format_specs_.uppercase = true; break;
            case 's': format_specs_.representation = ash::FormatFlags::Repr::shorthand; break;

            default : continue;
            case '}': return spec;
         }
      }
      return ctx.end( );
   }

   constexpr auto format( ash::Signal sig, std::format_context& ctx ) const noexcept
   {
      return std::format_to( ctx.out( ), "{}", ash::reflection::to_string( sig, format_specs_ ) );
   }

private:
   ash::FormatFlags format_specs_{};
};
