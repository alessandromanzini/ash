#ifndef MONGOOSE_CBRIDGE_HPP
#define MONGOOSE_CBRIDGE_HPP


#if defined( _MSC_VER )

   #define CBRIDGE_DEBUG_BREAK( ) __debugbreak( )

#elif defined( __clang__ ) || defined( __GNUC__ )

   #define CBRIDGE_DEBUG_BREAK( ) __builtin_debugtrap( )

#else

   #include <csignal>
   #define CBRIDGE_DEBUG_BREAK( ) std::raise( SIGTRAP )

#endif


namespace ash::cbridge
{

   // std::tm tm{};
   // #if defined( _WIN32 )
   // localtime_s( &tm, &meta.timestamp );
   // #else
   // localtime_r( &meta.timestamp, &tm );
   // #endif
}


#endif //!MONGOOSE_CBRIDGE_HPP
