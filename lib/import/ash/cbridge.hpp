#ifndef ASH_CBRIDGE_HPP
#define ASH_CBRIDGE_HPP


#if defined( _MSC_VER )

   #define CBR_DEBUG_BREAK( ) __debugbreak( )
   #define CBR_FORCE_INLINE __forceinline

#elif defined( __clang__ ) || defined( __GNUC__ )

   #define CBR_DEBUG_BREAK( ) __builtin_debugtrap( )
   #define CBR_FORCE_INLINE [[gnu::always_inline]] inline

#else

   #include <csignal>
   #define CBR_DEBUG_BREAK( ) std::raise( SIGTRAP )
   #define CBR_FORCE_INLINE

#endif


#endif //!ASH_CBRIDGE_HPP
