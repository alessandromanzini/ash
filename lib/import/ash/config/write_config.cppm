module;

#include <ash/pch.hpp>

export module ash:log_config;


export namespace ash::cfg
{
   struct WriteConfig
   {
      enum class TitleInjection : uint8_t { none, per_block };

      std::reference_wrapper<std::ostream> log_stream = std::clog;
      std::reference_wrapper<std::ostream> err_stream = std::cerr;
      TitleInjection title_injection = TitleInjection::per_block;
      std::string identity = "ASH";
      std::string title_fmt = "[{SIG:u}] [{ID}] at {PATH}:{LINE}:{COL}"; //[{TIME:%H:%M:%S}]
      std::string trailer_content = "\n\n";
   };

   enum class FlushAction : uint8_t { carry_on, end };
}
