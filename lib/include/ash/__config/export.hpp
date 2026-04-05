#ifndef ASH_EXPORT_HPP
#define ASH_EXPORT_HPP


#ifdef ASH_SHARED

    #ifdef _MSC_VER
        #ifdef ASH_BUILD_LIB
         #define ASH_EXPORT __declspec(dllexport)
        #else
         #define ASH_EXPORT __declspec(dllimport)
        #endif
    #else
        #define ASH_EXPORT __attribute__((__visibility__("default")))
    #endif

#elifndef ASH_STATIC

    #define ASH_EXPORT

#else

static_assert( false, "API type must be defined!" );

#endif


#endif //!ASH_EXPORT_HPP
