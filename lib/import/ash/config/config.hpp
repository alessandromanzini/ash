#ifndef ASH_CONFIG_HPP
#define ASH_CONFIG_HPP

#include <ash/pch.hpp>


namespace ash::cfg
{
   enum class PlatformType : std::uint8_t { unknown = 0, apple, windows, linux_os, ps4, ps5, switch_os, xbox_one };
   enum class CompilerType : std::uint8_t { clang, msvc, gnu };
   enum class ArchType : std::uint8_t { arm64, x86_64 };
}

namespace ash::cfg
{
#ifdef NDEBUG
   inline constexpr bool debug_mode{ false };
#else
   inline constexpr bool debug_mode{ true };
#endif

#ifdef ASH_BUILD_LIB
   inline constexpr bool lib_level{ true };
#else
   inline constexpr bool lib_level{ false };
#endif

#ifdef __ORBIS__      // Ps4
   inline constexpr auto platform_type = PlatformType::ps4;
#elifdef __PROSPERO__ // Ps5
   inline constexpr auto platform_type = PlatformType::ps5;
#elifdef __NX__       // Nintendo Switch
   inline constexpr auto platform_type = PlatformType::switch_os;
#elifdef _DURANGO     // Xbox One
   inline constexpr auto platform_type = PlatformType::xbox_one;
#elifdef __APPLE__    // Apple
   inline constexpr auto platform_type = PlatformType::apple;
#elifdef _WIN32       // Win32
   inline constexpr auto platform_type = PlatformType::windows;
#elifdef __linux__    // Linux
   inline constexpr auto platform_type = PlatformType::linux_os;
#else                 //!Unknown
   inline constexpr auto platform_type = PlatformType::unknown;
#endif

#ifdef __clang__
   inline constexpr auto compiler_type = CompilerType::clang;
#elifdef _MSC_VER
   inline constexpr auto compiler_type = CompilerType::msvc;
#elifdef __GNUC__
   inline constexpr auto compiler_type = CompilerType::gnu;
#else
   static_assert( false, "Unknown compiler!" );
#endif

#if defined( __aarch64__ ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
   inline constexpr auto arch_type = ArchType::arm64;
#elif defined( __x86_64__ ) || defined( _M_X64 )
   inline constexpr auto arch_type = ArchType::x86_64;
#else
   static_assert( false, "Unsupported architecture!" );
#endif
}


#endif //!ASH_CONFIG_HPP
