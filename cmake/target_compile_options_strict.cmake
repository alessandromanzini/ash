function( target_compile_options_strict TARGET_NAME )
    #
    if( MSVC )
        target_compile_options(
                ${TARGET_NAME} PRIVATE
                /W4                     # high warning level
                /WX                     # warnings as errors
                /permissive-            # strict conformance mode
                /w14062                 # enumerator not handled in switch
                /w14165                 # 'HRESULT' cast to 'bool'
                /w14242                 # possible loss of data (int -> char etc)
                /w14254                 # larger bit field assigned to smaller
                /w14263                 # member function doesn't override base
                /w14265                 # class has virtual functions but no virtual destructor
                /w14287                 # unsigned/negative constant mismatch
                /w14296                 # expression always false/true
                /w14311                 # pointer truncation
                /w14545                 # expression before comma has no effect
                /w14546                 # function call before comma has no effect
                /w14547                 # operator before comma has no effect
                /w14549                 # operator before comma has no effect
                /w14555                 # expression has no effect
                /w14619                 # pragma warning: no warning number
                /w14640                 # thread unsafe static member init
                /w14826                 # conversion is sign-extended
                /w14905                 # wide string literal cast to LPSTR
                /w14906                 # string literal cast to LPWSTR
                /w14928                 # illegal copy-initialization
                /EHs-                   # disable standard C++ exception handling (s-)
                /EHc-                   # disable extern "C" exceptions (c-)
                /GR-                    # disable RTTI
                /fp:precise             # safe floating point
                /Zc:__cplusplus         # report correct __cplusplus value
                /Zc:inline              # remove unreferenced functions
                /Zc:preprocessor        # conforming preprocessor
        )
        #
        # disable exceptions engine-wide
        target_compile_definitions( ${TARGET_NAME} PRIVATE _HAS_EXCEPTIONS=0 )
        #
    else() # GCC / Clang
        target_compile_options(
                ${TARGET_NAME} PRIVATE
                -Wall                   # common useful warnings
                -Wextra                 # extra warning for sloppy/weird code
                -Wpedantic              # warnings for non-standard compliant code
                -Werror                 # treat warnings as errors
                -Wshadow                # variable shadows outer scope
                -Wnon-virtual-dtor      # base class with virtual funcs but no virtual dtor
                -Wold-style-cast        # C-style casts
                -Wcast-align            # potential performance cast
                -Wunused                # anything unused
                -Woverloaded-virtual    # overloaded (not overridden) virtual
                -Wconversion            # implicit conversions that may lose data
                -Wsign-conversion       # signed/unsigned conversion
                -Wdouble-promotion      # float implicitly promoted to double
                -Wformat=2              # printf/scanf format string issues
                -Wno-unknown-pragmas    # ignore MSVC pragmas in shared headers
                -fno-exceptions         # disable exceptions
                -fno-rtti               # disable RTTI
                -ffast-math             # fast floating point (equiv to /fp:fast)
        )
    endif()
    #
    message( STATUS "${TARGET_NAME}: strict compilation options applied." )
    #
endfunction()