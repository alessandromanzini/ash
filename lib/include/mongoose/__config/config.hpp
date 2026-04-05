#ifndef MONGOOSE_CONFIG_HPP
#define MONGOOSE_CONFIG_HPP


namespace mongoose
{
   enum class PlatformType
   {
      unknown = 0, apple, windows, linux_os,
      ps4, ps5, switch_os, xbox_one
   };

   enum class CompilerType
   {
      clang, msvc, gnu
   };
}

namespace mongoose
{
   // +--------------------------------+
   // | DEBUG MODE                     |
   // +--------------------------------+
#ifdef NDEBUG
   inline constexpr bool debug_mode = false;
#else
   inline constexpr bool debug_mode = true;
#endif

   // +--------------------------------+
   // | PLATFORM TYPE                  |
   // +--------------------------------+
#ifdef __ORBIS__        // PS4
   inline constexpr auto platform_type = PlatformType::ps4;
#elifdef __PROSPERO__   // PS5
   inline constexpr auto platform_type = PlatformType::ps5;
#elifdef __NX__         // Nintendo Switch
   inline constexpr auto platform_type = PlatformType::switch_os;
#elifdef _DURANGO       // Xbox One
   inline constexpr auto platform_type = PlatformType::xbox_one;
#elifdef __APPLE__
   inline constexpr auto platform_type = PlatformType::apple;
#elifdef _WIN32
   inline constexpr auto platform_type = PlatformType::windows;
#elifdef __linux__
   inline constexpr auto platform_type = PlatformType::linux_os;
#else
   inline constexpr auto platform_type = PlatformType::unknown;
#endif

   // +--------------------------------+
   // | COMPILER                       |
   // +--------------------------------+
#ifdef __clang__
   inline constexpr auto compiler_type = CompilerType::clang;
#elifdef _MSC_VER
   inline constexpr auto compiler_type = CompilerType::msvc;
#elifdef __GNUC__
   inline constexpr auto compiler_type = CompilerType::gnu;
#else
   static_assert(false, "Unknown compiler!");
#endif
}


#endif //!MONGOOSE_CONFIG_HPP
