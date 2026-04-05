#ifndef MONGOOSE_EXPORT_HPP
#define MONGOOSE_EXPORT_HPP


#ifdef MONGOOSE_SHARED

    #ifdef _MSC_VER
        #ifdef SWIFTGUARD_BUILD_LIB
         #define MONGOOSE_EXPORT __declspec(dllexport)
        #else
         #define MONGOOSE_EXPORT __declspec(dllimport)
        #endif
    #else
        #define MONGOOSE_EXPORT __attribute__((__visibility__("default")))
    #endif

#elifndef MONGOOSE_STATIC

    #define MONGOOSE_EXPORT

#else

static_assert( false, "API type must be defined!" );

#endif


#endif //!MONGOOSE_EXPORT_HPP
