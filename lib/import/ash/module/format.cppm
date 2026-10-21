module;

#include <ash/pch.hpp>

#include <bhop/cbridge.hpp>

export module ash:format;

import bhop;

import :format_flags;
import :signal;
import :time;


namespace ash::detail
{
   /**
    * Deliberately not constexpr: reaching it while a title is parsed aborts constant evaluation, and the compiler reports the call, \p reason
    * included, as the error.
    */
   inline auto invalid_format( char const* /*reason*/ ) noexcept -> void { }
}

namespace ash::detail
{
   /**
    * One parsed piece of a title: either literal text, or a keyword expanded from the entry's metadata.
    */
   struct Field
   {
      enum class Kind : std::uint8_t { literal, signal, identity, path, line, column, function, time_quick, time_generic };

      Kind kind{ Kind::literal };
      std::string_view text{}; // literal: the verbatim text; time_generic: the chrono spec.
      FormatFlags signal_flags{};
      time::HmsPrecision precision{};
   };

   // Longest chrono spec that still fits the render-time "{:spec}" scratch buffer.
   constexpr std::size_t max_time_spec_length{ 61 };

   [[nodiscard]] consteval auto parse_signal( std::string_view spec ) -> Field
   {
      FormatFlags flags{};
      for ( char const flag : spec )
      {
         switch ( flag )
         {
            case 'u': flags.uppercase = true; break;
            case 's': flags.representation = FormatFlags::Repr::shorthand; break;
            default : invalid_format( "SIG spec only accepts u and s." );
         }
      }
      return { .kind = Field::Kind::signal, .signal_flags = flags };
   }

   [[nodiscard]] consteval auto parse_time( std::string_view spec ) -> Field
   {
      std::string_view const time_spec{ spec.empty( ) ? "%H:%M:%S" : spec };
      //
      // hms-shaped specs dispatch to the quick table renderer, everything else is forwarded to the chrono formatter.
      if ( std::optional<time::HmsPrecision> const precision{ time::parse_hms_spec( time_spec ) }; precision.has_value( ) )
      {
         return { .kind = Field::Kind::time_quick, .precision = precision.value( ) };
      }
      if ( time_spec.size( ) > max_time_spec_length )
      {
         invalid_format( "TIME spec is too long" );
      }
      return { .kind = Field::Kind::time_generic, .text = time_spec };
   }

   /**
    * Parse the content of one "{KEYWORD[:spec]}" token, braces excluded.
    */
   [[nodiscard]] consteval auto parse_field( std::string_view token ) -> Field
   {
      std::size_t const colon{ token.find( ':' ) };
      std::string_view const name{ token.substr( 0, colon ) };
      std::string_view const spec{ colon == std::string_view::npos ? std::string_view{} : token.substr( colon + 1 ) };
      //
      // 1. Keywords that accept a spec.
      if ( name == "SIG" )
      {
         return parse_signal( spec );
      }
      if ( name == "TIME" )
      {
         return parse_time( spec );
      }
      //
      // 2. Keywords that do not.
      struct Keyword
      {
         std::string_view name;
         Field::Kind kind;
      };
      constexpr std::array keywords{
         Keyword{       "ID", Field::Kind::identity },
         Keyword{     "PATH",     Field::Kind::path },
         Keyword{     "LINE",     Field::Kind::line },
         Keyword{   "COLUMN",   Field::Kind::column },
         Keyword{ "FUNCTION", Field::Kind::function },
      };
      for ( auto const& [keyword, kind] : keywords )
      {
         if ( name == keyword )
         {
            if ( colon != std::string_view::npos )
            {
               invalid_format( "Keyword does not take a spec." );
            }
            return { .kind = kind };
         }
      }
      invalid_format( "Unknown keyword." );
      return {};
   }
}

export namespace ash::schema
{
   /**
    * Per-entry information captured at the call site, consumed by the title renderer. Per-logger information (e.g. its identity) lives in the
    * \c WriteConfig instead, so entries stay small and own nothing.
    */
   struct WriteMetadata
   {
      using sequence_id_type = std::uint32_t;

      sequence_id_type sequence_id;
      Signal signal;
      time::TickClock::repr timestamp;
      std::source_location where;
   };
}

export namespace ash
{
   /**
    * @brief A log title, parsed at compile time into fields that are expanded from each entry's metadata.
    *
    * Like \c std::format_string, it is built implicitly from a string literal, and a malformed title is a compile error rather than a surprise in the
    * output. Literal text is kept as views into the literal, so rendering is a walk over the fields with no parsing left.
    *
    * Supported keywords:
    *  - `{TIME}`     — Timestamp of the entry, formatted using `std::chrono` format specs (e.g. `{TIME:%H:%M:%S}`). It is recommended to use the
    *                   quick table specs below, any other chrono spec is supported but processed significantly slower:
    *                    - "HH"
    *                    - "HH:MM"
    *                    - "HH:MM:SS"
    *                    - "HH:MM:SS.mmm"
    *                    - "HH:MM:SS.uuuuuu"
    *                    - "%H:%M:%S" (default)
    *  - `{SIG}`      — Severity level of the entry. `{SIG:u}` for uppercase, `{SIG:s}` for shorthand; flags can be combined (`{SIG:us}`).
    *  - `{ID}`       — Name of the logger, see \c WriteConfig::identity.
    *  - `{PATH}`     — Source file path of the call site.
    *  - `{LINE}`     — Source line number of the call site.
    *  - `{COLUMN}`   — Source column number of the call site.
    *  - `{FUNCTION}` — Enclosing function name of the call site.
    *
    * @note Keywords and specs are case-sensitive. Use "{{" and "}}" for literal braces.
    */
   class CompiledFormat final
   {
   public:
      static constexpr std::size_t max_fields{ 32 };

      // Implicit by design, so a string literal can be assigned wherever a title is expected.
      template <typename S> requires std::convertible_to<S const&, std::string_view>
      consteval CompiledFormat( S const& source )
      {
         compile( source );
      }

      /**
       * @param reservoir
       * @param metadata
       * @param identity
       * @param timestamp The entry's wall-clock time, already converted from \c WriteMetadata::timestamp ticks.
       */
      template <std::size_t N>
      auto render( bhop::CharReservoir<char, N>& reservoir, schema::WriteMetadata const& metadata, std::string_view identity,
                   std::chrono::system_clock::time_point timestamp ) const -> void
      {
         using Kind = detail::Field::Kind;
         //
         for ( auto const& [kind, text, signal_flags, precision] : std::span{ fields_.data( ), field_count_ } )
         {
            switch ( kind )
            {
               case Kind::literal     : reservoir.pour( text ); break;
               case Kind::signal      : reservoir.pour( reflection::to_string( metadata.signal, signal_flags ) ); break;
               case Kind::identity    : reservoir.pour( identity ); break;
               case Kind::path        : reservoir.pour( std::string_view{ metadata.where.file_name( ) } ); break;
               case Kind::line        : reservoir.pour( metadata.where.line( ) ); break;
               case Kind::column      : reservoir.pour( metadata.where.column( ) ); break;
               case Kind::function    : reservoir.pour( std::string_view{ metadata.where.function_name( ) } ); break;

               case Kind::time_quick  : time::visit_hms( precision, timestamp, [&]( std::string_view hms ) { reservoir.pour( hms ); } ); break;

               case Kind::time_generic: render_generic_time( reservoir, text, timestamp ); break;

               default                : std::unreachable( );
            }
         }
      }

   private:
      std::array<detail::Field, max_fields> fields_{};
      std::size_t field_count_{ 0 };

      consteval auto compile( std::string_view fmt ) -> void
      {
         std::size_t literal_begin{ 0 };
         std::size_t cursor{ 0 };
         //
         while ( cursor < fmt.size( ) )
         {
            char const c{ fmt[cursor] };
            //
            // 1. Plain character: keep growing the current literal run.
            if ( c != '{' && c != '}' )
            {
               ++cursor;
               continue;
            }
            //
            // 2. Escaped brace ("{{" or "}}"): close the literal run including a single brace, then skip both.
            if ( cursor + 1 < fmt.size( ) && fmt[cursor + 1] == c )
            {
               push_literal( fmt.substr( literal_begin, cursor + 1 - literal_begin ) );
               cursor += 2;
            }
            // 3. Keyword: close the literal run, then parse the "{KEYWORD[:spec]}" token.
            else
            {
               push_literal( fmt.substr( literal_begin, cursor - literal_begin ) );
               //
               if ( c == '}' )
               {
                  detail::invalid_format( "Unmatched }." );
               }
               std::size_t const close{ fmt.find( '}', cursor + 1 ) };
               if ( close == std::string_view::npos )
               {
                  detail::invalid_format( "Missing } after keyword." );
               }
               push( detail::parse_field( fmt.substr( cursor + 1, close - cursor - 1 ) ) );
               cursor = close + 1;
            }
            literal_begin = cursor;
         }
         //
         // 4. Trailing literal run.
         push_literal( fmt.substr( literal_begin ) );
      }

      consteval auto push( detail::Field const& field ) -> void
      {
         if ( field_count_ == max_fields )
         {
            detail::invalid_format( "Too many fields." );
         }
         fields_[field_count_++] = field;
      }

      consteval auto push_literal( std::string_view text ) -> void
      {
         if ( not text.empty( ) )
         {
            push( { .kind = detail::Field::Kind::literal, .text = text } );
         }
      }

      /**
       * The chrono formatter only takes a full replacement field, so the spec is wrapped as "{:spec}" in a stack buffer first.
       */
      template <std::size_t N>
      static auto render_generic_time( bhop::CharReservoir<char, N>& reservoir, std::string_view spec, std::chrono::system_clock::time_point timestamp )
        -> void
      {
         std::array<char, detail::max_time_spec_length + 3> fmt{ '{', ':' };
         auto const spec_end = std::ranges::copy( spec, std::next( fmt.begin( ), 2 ) ).out;
         *spec_end = '}';
         //
         std::string_view const replacement_field{ fmt.data( ), spec.size( ) + 3 };
         std::vformat_to( reservoir.inserter( ), replacement_field, std::make_format_args( timestamp ) );
      }
   };
}

export namespace ash
{
   /**
    * @brief A format string that also records where it was written.
    *
    * The \c std::source_location is a default argument of the constructor, so it is evaluated where the string literal is converted: the caller's
    * line. Format strings are still checked at compile time, exactly like \c std::format_string.
    */
   template <typename... Args> struct FormatAt
   {
      std::format_string<Args...> fmt;
      std::source_location where;

      template <typename S> requires std::convertible_to<S const&, std::string_view>
      consteval FormatAt( S const& source, std::source_location const& location = std::source_location::current( ) )
         : fmt{ source }
         , where{ location }
      { }
   };
}
