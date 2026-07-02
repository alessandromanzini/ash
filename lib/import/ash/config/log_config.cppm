module;

#include <ash/pch.hpp>

export module ash:log_config;


export namespace ash::cfg
{
   struct LogConfig
   {
      std::string identity = "LOGGER";
      std::reference_wrapper<std::ostream> log_stream = std::clog;
      std::reference_wrapper<std::ostream> err_stream = std::cerr;
   };

   enum class FlushAction : uint8_t { carry_on, end };
}
