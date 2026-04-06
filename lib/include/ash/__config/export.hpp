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

#else

   #define ASH_EXPORT

#endif


#endif //!ASH_EXPORT_HPP
