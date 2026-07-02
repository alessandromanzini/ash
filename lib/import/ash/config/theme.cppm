module;

#include <ash/pch.hpp>

#include <ash/config/theme.hpp>

export module ash:theme;

import :signal;


// The theme value types are defined in the plain header above (attached to the global module) so
// the Objective-C++ platform layer can share them. Re-export the names for `import ash;` consumers.
export namespace ash::theme
{
   using ash::theme::Color;
   using ash::theme::signal_to_color;
}
