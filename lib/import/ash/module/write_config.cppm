module;

#include <ash/pch.hpp>

export module ash:write_config;

import :format;


export namespace ash::schema
{
   /**
    * @brief Configuration for a stream writer.
    */
   struct WriteConfig
   {
      /// Controls how often the title is (re-)injected into the output.
      /// - `none`: the title is never written.
      /// - `per_block`: the title is written once at the start of each entry.
      enum class TitleInjection : std::uint8_t { none, per_block };

      /// Stream used for regular (informational) log output.
      std::reference_wrapper<std::ostream> log_stream = std::clog;

      /// Stream used for error-level output.
      std::reference_wrapper<std::ostream> err_stream = std::cerr;

      /// Determines when @ref title_format is expanded and written.
      TitleInjection title_injection = TitleInjection::per_block;

      /// Title written before each entry, parsed at compile time; see \p Format for the supported keywords.
      Format title_format = "[{TIME:%H:%M:%S}] [{SIG:u}] [{ID}] at {PATH}:{LINE}:{COLUMN}";

      /// Content appended after each write (e.g. block separator).
      std::string trailer_content = "\n\n";
   };
}
