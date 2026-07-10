#ifndef ASH_CBRIDGE_HPP
#define ASH_CBRIDGE_HPP


#if defined( _MSC_VER )

   #define CBR_DEBUG_BREAK( ) __debugbreak( )
   #define CBR_FORCE_INLINE __forceinline
   #define CBR_NO_INLINE __declspec(noinline)

#elif defined( __clang__ ) || defined( __GNUC__ )

   #define CBR_DEBUG_BREAK( ) __builtin_debugtrap( )
   #define CBR_FORCE_INLINE [[gnu::always_inline]] inline
   #define CBR_NO_INLINE [[gnu::noinline]]

#else

   #include <csignal>
   #define CBR_DEBUG_BREAK( ) std::raise( SIGTRAP )
   #define CBR_FORCE_INLINE
   #define CBR_NO_INLINE

#endif


#endif //!ASH_CBRIDGE_HPP
