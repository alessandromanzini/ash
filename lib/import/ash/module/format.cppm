module;

#include <ash/pch.hpp>

#include <ash/cbridge.hpp>

export module ash:format;

import :reservoir;

import :format_flags;
import :signal;
import :time;
import :write_config;


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

namespace ash::detail
{
   /**
    * A compiled title segment.
    */
   struct Field
   {
      enum class Kind : uint8_t { literal, signal, identity, path, line, column, function, quick_time, generic_time } kind;
      std::string_view verbatim{};
      FormatFlags specs{};
      schema::HmsPrecision precision{};
   };

   [[nodiscard]] inline auto parse( std::string_view token ) noexcept -> std::optional<Field>
   {
      size_t const colon = token.find( ':' );
      std::string_view const name = token.substr( 0, colon );
      std::string_view const spec = colon == std::string_view::npos ? std::string_view{} : token.substr( colon + 1 );
      //
      if ( name == "SIG" )
      {
         FormatFlags flags{};
         for ( char const flag : spec )
         {
            switch ( flag )
            {
               case 'u': flags.uppercase = true; break;
               case 's': flags.representation = FormatFlags::Repr::shorthand; break;
               default : break;
            }
         }
         return Field{ .kind = Field::Kind::signal, .specs = flags };
      }
      if ( name == "ID" )
      {
         return Field{ .kind = Field::Kind::identity };
      }
      if ( name == "PATH" )
      {
         return Field{ .kind = Field::Kind::path };
      }
      if ( name == "LINE" )
      {
         return Field{ .kind = Field::Kind::line };
      }
      if ( name == "COLUMN" )
      {
         return Field{ .kind = Field::Kind::column };
      }
      if ( name == "FUNCTION" )
      {
         return Field{ .kind = Field::Kind::function };
      }
      if ( name == "TIME" )
      {
         std::string_view const time_spec = spec.empty( ) ? "%H:%M:%S" : spec;
         //
         // hms-shaped specs dispatch to the quick table renderer, otherwise forwarded to the generic formatter.
         if ( std::optional<schema::HmsPrecision> const precision = schema::parse_hms_spec( time_spec ); precision.has_value( ) )
         {
            return Field{ .kind = Field::Kind::quick_time, .precision = precision.value( ) };
         }
         return Field{ .kind = Field::Kind::generic_time, .verbatim = time_spec };
      }
      return std::nullopt;
   }

   template <typename ClockT, std::invocable<std::string_view> V>
   auto visit_hms( schema::HmsPrecision precision, std::chrono::time_point<ClockT> timestamp, V&& visitor ) -> void
   {
      switch ( precision )
      {
         case schema::HmsPrecision::hrs  : visitor( time::render_hms<std::chrono::hours>( timestamp ).view( ) ); break;
         case schema::HmsPrecision::min  : visitor( time::render_hms<std::chrono::minutes>( timestamp ).view( ) ); break;
         case schema::HmsPrecision::sec  : visitor( time::render_hms<std::chrono::seconds>( timestamp ).view( ) ); break;
         case schema::HmsPrecision::milli: visitor( time::render_hms<std::chrono::milliseconds>( timestamp ).view( ) ); break;
         case schema::HmsPrecision::micro: visitor( time::render_hms<std::chrono::microseconds>( timestamp ).view( ) ); break;
         default                         : std::unreachable( );
      }
   }

   inline auto make_generic_format( std::string_view spec ) -> std::string
   {
      std::string format;
      format.reserve( spec.length( ) + 3 );
      format.push_back( '{' );
      format.push_back( ':' );
      format.append( spec );
      format.push_back( '}' );
      return format;
   }
}

export namespace ash
{
   /**
    * @brief A pre-compiled title representation for quick formatting.
    *
    * @note Assumes \c fmt view stays alive for the whole \p CompiledFormat lifetime.
    */
   class CompiledFormat final
   {
   public:
      CompiledFormat( ) = default;
      CBR_FORCE_INLINE explicit CompiledFormat( std::string_view fmt ) noexcept { compile( fmt ); }

      template <size_t N> CBR_FORCE_INLINE auto render( CharReservoir<char, N>& reservoir, schema::WriteMetadata const& metadata ) const -> void
      {
         // ReSharper disable once CppUseStructuredBinding
         for ( auto const& field : fields_ )
         {
            switch ( field.kind )
            {
               case detail::Field::Kind::literal : reservoir.pour( field.verbatim ); break;
               case detail::Field::Kind::signal  : reservoir.pour( reflection::to_string( metadata.signal, field.specs ) ); break;
               case detail::Field::Kind::identity: reservoir.pour( metadata.identity ); break;
               case detail::Field::Kind::path    : reservoir.pour( std::string_view{ metadata.options.where.file_name( ) } ); break;
               case detail::Field::Kind::line    : reservoir.pour( metadata.options.where.line( ) ); break;
               case detail::Field::Kind::column  : reservoir.pour( metadata.options.where.column( ) ); break;
               case detail::Field::Kind::function: reservoir.pour( std::string_view{ metadata.options.where.function_name( ) } ); break;

               case detail::Field::Kind::quick_time:
                  detail::visit_hms( field.precision, metadata.timestamp, [&]( std::string_view hms ) { reservoir.pour( hms ); } );
                  break;

               case detail::Field::Kind::generic_time:
                  std::vformat_to(
                    reservoir.inserter( ), detail::make_generic_format( field.verbatim ), std::make_format_args( metadata.timestamp ) );
                  break;

               default: std::unreachable( );
            }
         }
      }

   private:
      std::vector<detail::Field> fields_{};

      auto compile( std::string_view fmt ) noexcept -> void
      {
         size_t literal_index = 0;
         size_t literal_length = 0;
         //
         auto const append_literal = [&]( size_t index, size_t amount = 1 ) {
            if ( literal_length == 0 )
            {
               literal_index = index;
               literal_length = amount;
            }
            else
            {
               literal_length += amount;
            }
         };
         auto const flush_literal = [&] {
            if ( literal_length > 0 )
            {
               fields_.push_back( detail::Field{ .kind = detail::Field::Kind::literal, .verbatim = fmt.substr( literal_index, literal_length ) } );
               literal_length = 0;
            }
         };
         //
         // For every character in the format string ...
         for ( size_t char_index = 0; char_index < fmt.size( ); )
         {
            char const c = fmt[char_index];
            //
            // 1. Escaped braces: {{ and }}. Collapse to a single literal brace.
            if ( bool const is_double_brace = ( c == '{' || c == '}' ) && char_index + 1 < fmt.size( ) && fmt[char_index + 1] == c; is_double_brace )
            {
               append_literal( char_index, 2 );
            }
            // 2. Opening brace (guaranteed to be single) ...
            else if ( c == '{' )
            {
               // ... query for the ending brace and ...
               size_t const close = fmt.find( '}', char_index + 1 );
               //
               // ... if it doesn't exist, keep the remainder as literal text, ...
               if ( close == std::string_view::npos )
               {
                  append_literal( char_index, fmt.size( ) - char_index );
                  break;
               }
               //
               // ... otherwise parse its content into a segment. Also flush the literals collected thus far.
               size_t const substr_begin_index = char_index + 1;
               std::string_view const token = fmt.substr( substr_begin_index, close - char_index - 1 );
               if ( std::optional<detail::Field> field = detail::parse( token ); field.has_value( ) )
               {
                  flush_literal( );
                  fields_.push_back( field.value( ) );
               }
               else
               {
                  // If the field was unknown, keep it verbatim, braces included, so the typo is visible in the output.
                  append_literal( substr_begin_index, token.size( ) );
               }
               char_index = close + 1;
            }
            // 3. Default case: push to literal space.
            else
            {
               append_literal( char_index );
               ++char_index;
            }
         }
         //
         // Finally flush any residue literals.
         flush_literal( );
      }
   };
}
