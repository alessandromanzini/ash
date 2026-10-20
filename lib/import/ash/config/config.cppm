module;

#include <ash/config/config.hpp>

export module ash:config;


export namespace ash::cfg
{
   using cfg::PlatformType;
   using cfg::CompilerType;
}

export namespace ash::cfg
{
   using cfg::debug_mode;
   using cfg::lib_level;
   using cfg::platform_type;
   using cfg::compiler_type;
   using cfg::arch_type;
}
