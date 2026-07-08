module;

#include <ash/pch.hpp>

export module ash:format;


export namespace ash
{
   /**
    * The \p FormatBundle represents a string format and args as a single packet, making it easier to pass it around along with other optional and
    * variadic parameters.
    * @tparam TArgs
    */
   template <typename... TArgs> struct FormatBundle
   {
      static constexpr bool empty_args = sizeof...( TArgs ) == 0U;

      std::format_string<TArgs...> fmt;
      std::tuple<std::decay_t<TArgs>...> args;

      constexpr FormatBundle( std::format_string<TArgs...> fmt, TArgs&&... args ) noexcept
         : fmt{ fmt }
      , args{ std::forward<TArgs>( args )... }
      { }

      [[nodiscard]] constexpr auto format( ) const noexcept -> std::string
      {
         if constexpr ( not empty_args )
         {
            return std::apply(
              [this]( auto const&... fmt_args ) -> auto { return std::vformat( fmt.get( ), std::make_format_args( fmt_args... ) ); }, args );
         }
         else
         {
            return std::string{ fmt.get( ) };
         }
      }

      template <std::output_iterator<char const&> TStrIterator> constexpr auto format_to( TStrIterator it ) const noexcept -> TStrIterator
      {
         if constexpr ( not empty_args )
         {
            return std::apply(
              [&]( auto const&... fmt_args ) -> auto { return std::vformat_to( it, fmt.get( ), std::make_format_args( fmt_args... ) ); }, args );
         }
         else
         {
            return std::copy( fmt.get( ).begin( ), fmt.get( ).end( ), it );
         }
      }
   };

   template <typename... TArgs> [[nodiscard]] auto format( std::format_string<TArgs...> fmt, TArgs&&... args ) noexcept -> FormatBundle<TArgs...>
   {
      return FormatBundle<TArgs...>{ fmt, std::forward<TArgs>( args )... };
   }
}
