module;

#include <ash/pch.hpp>

export module ash:write_config;

import :signal;


export namespace ash::schema
{
   /**
   * @brief Configuration for a stream writer.
   *
   * @note The \p title_format support certain keywords to inject information while writing.
   *       Below, the list of keywords:
   *
   *       - `{TIME}`      — Timestamp of write request, formatted using `std::chrono` format specs (e.g. `{TIME:%H:%M:%S}`).
   *                         If no spec is given, a default representation is used. It is recommended to use the quick table specs defined as follows:
   *                          - "HH"
   *                          - "HH:MM"
   *                          - "HH:MM:SS"
   *                          - "HH:MM:SS.mmm"
   *                          - "HH:MM:SS.uuuuuu"
   *                          - "%H:%M:%S" (default)
   *                         Any other standard library format spec is supported, but will be processed significantly slower.
   *
   *       - `{SIG}`       — Severity level of the entry. Supports a case modifier:
   *                         `{SIG:u}` for uppercase, `{SIG:s}` for shorthand version; omitting the spec uses the level's default casing and length.
   *
   *       - `{ID}`        — Unique identifier of the entry.
   *
   *       - `{PATH}`      — Source file path of the call site.
   *
   *       - `{LINE}`      — Source line number of the call site.
   *
   *       - `{COLUMN}`    — Source column number of the call site.
   *
   *       - `{FUNCTION}`  — Source column number of the call site.
   *
   * @note Keywords and specs are case-sensitive and must appear exactly as `{KEYWORD}` or `{KEYWORD:spec}`; unrecognized keywords are left unexpanded
   *       in the output.
   */
   struct WriteConfig
   {
      /// Controls how often the title is (re-)injected into the output.
      /// - `none`: the title is never written.
      /// - `per_block`: the title is written once at the start of each entry.
      enum class TitleInjection : uint8_t { none, per_block }; // TODO: Implement this

      /// Stream used for regular (informational) log output.
      std::reference_wrapper<std::ostream> log_stream = std::clog;

      /// Stream used for error-level output.
      std::reference_wrapper<std::ostream> err_stream = std::cerr;

      /// Determines when @ref title_format is expanded and written.
      TitleInjection title_injection = TitleInjection::per_block;

      /// Format string for the injected title; see the keyword list above.
      std::string title_format = "[{TIME:%H:%M:%S}] [{SIG:u}] [{ID}] at {PATH}:{LINE}:{COLUMN}";

      /// Content appended after each write (e.g. block separator).
      std::string trailer_content = "\n\n";
   };

   struct WriteOptions
   {
      std::optional<uint32_t> echo; // TODO: implement echo
      std::source_location where;
   };

   struct WriteMetadata // TODO: identifier
   {
      using sequence_id_type = uint32_t;

      sequence_id_type sequence_id;
      Signal signal;
      std::string_view identity;
      std::chrono::system_clock::time_point timestamp;
      WriteOptions options;
   };
}

export namespace ash::schema
{
   /**
    * Runtime-selectable precision for the fixed "HH[:MM[:SS[.x]]]" renderer.
    */
   enum class HmsPrecision : std::uint8_t { hrs, min, sec, milli, micro };

   struct HmsMapping
   {
      std::string_view spec;
      HmsPrecision precision;
   };

   constexpr auto hms_map = [] {
      return std::array{
         HmsMapping{              "HH",   HmsPrecision::hrs },
         HmsMapping{           "HH:MM",   HmsPrecision::min },
         HmsMapping{        "HH:MM:SS",   HmsPrecision::sec },
         HmsMapping{    "HH:MM:SS.mmm", HmsPrecision::milli },
         HmsMapping{ "HH:MM:SS.uuuuuu", HmsPrecision::micro },
         HmsMapping{        "%H:%M:%S", HmsPrecision::milli },
      };
   }( );

   [[nodiscard]] constexpr auto parse_hms_spec( std::string_view spec ) noexcept -> std::optional<HmsPrecision>
   {
      for ( auto const& [candidate, precision] : hms_map )
      {
         if ( spec == candidate )
         {
            return precision;
         }
      }
      return std::nullopt;
   }
}
