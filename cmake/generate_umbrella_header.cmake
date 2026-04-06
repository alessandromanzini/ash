include(get_ms_timestamp)

function( generate_umbrella_header )
    #
    get_ms_timestamp( T_START_MS )
    #
    #
    # ========================================
    # ARGUMENT VALIDATION
    # ========================================
    #
    cmake_parse_arguments( ARG "" "UMBRELLA_HEADER_NAME;INCLUDE_DIRECTORY;OUTPUT_DIRECTORY" "EXCEPTION_REGEX" ${ARGN} )
    #
    get_filename_component( EXTENSION ${ARG_UMBRELLA_HEADER_NAME} EXT )
    if( NOT ${EXTENSION} STREQUAL "" )
        message( FATAL_ERROR "generate_umbrella_header: UMBRELLA_HEADER_NAME must not contain an extension (${ARG_UMBRELLA_HEADER_NAME})!" )
    endif()
    #
    if( NOT IS_ABSOLUTE ${ARG_INCLUDE_DIRECTORY} )
        message( FATAL_ERROR "generate_umbrella_header: INCLUDE_DIRECTORY must be an absolute path (${ARG_INCLUDE_DIRECTORY})!" )
    endif()
    #
    #
    # ========================================
    # HEADER GENERATION
    # ========================================
    #
    string( TOUPPER ${ARG_UMBRELLA_HEADER_NAME} HEADER_GUARD )
    string( APPEND HEADER_GUARD _HPP )
    set( UMBRELLA_HEADER_FILE ${ARG_UMBRELLA_HEADER_NAME}.hpp )
    #
    # include guards
    set( CONTENT "// UMBRELLA HEADER: auto-generated in cmake from globbed headers...\n\n" )
    string( APPEND CONTENT "#ifndef ${HEADER_GUARD}\n" )
    string( APPEND CONTENT "#define ${HEADER_GUARD}\n\n\n" )
    #
    # glob, exclude umbrella, sort
    file( GLOB_RECURSE INCLUDE_FILE_PATHS "${ARG_INCLUDE_DIRECTORY}/*.hpp" )
    list( FILTER INCLUDE_FILE_PATHS EXCLUDE REGEX ".*${UMBRELLA_HEADER_FILE}$" )
    foreach( REGEX ${ARG_EXCEPTION_REGEX} )
        list( FILTER INCLUDE_FILE_PATHS EXCLUDE REGEX "${REGEX}" )
    endforeach()
    list( SORT INCLUDE_FILE_PATHS COMPARE NATURAL ORDER ASCENDING )
    #
    # define appen_header macro
    macro( append_header PATH )
        file( RELATIVE_PATH REL_PATH ${ARG_INCLUDE_DIRECTORY} ${PATH} )
        string( APPEND CONTENT "#include \"${REL_PATH}\"\n" )
    endmacro()
    #
    # extract pch and ...
    set( PCH_FILE_PATHS ${INCLUDE_FILE_PATHS} )
    list( FILTER PCH_FILE_PATHS INCLUDE REGEX ".*/pch\\.hpp$" )
    #
    # ... if any was found, exclude from list and append in front
    list( LENGTH PCH_FILE_PATHS PCH_COUNT )
    if( PCH_COUNT GREATER 0 )
        list( FILTER INCLUDE_FILE_PATHS EXCLUDE REGEX ".*/pch\\.hpp$" )
        list( GET PCH_FILE_PATHS 0 PCH_FILE_PATH )
        append_header( ${PCH_FILE_PATH} )
        string( APPEND CONTENT "\n" )
    endif()
    #
    # append remaining headers
    foreach( INCLUDE_FILE_PATH ${INCLUDE_FILE_PATHS} )
        append_header( ${INCLUDE_FILE_PATH} )
    endforeach()
    #
    string( APPEND CONTENT "\n\n#endif //!${HEADER_GUARD}\n" )
    #
    cmake_path( APPEND ARG_OUTPUT_DIRECTORY ${UMBRELLA_HEADER_FILE} OUTPUT_VARIABLE OUTPUT_FILE_PATH )
    file( WRITE ${OUTPUT_FILE_PATH} ${CONTENT} )
    #
    #
    # ========================================
    # LOG STATUS
    # ========================================
    #
    get_ms_timestamp( T_END_MS )
    math( EXPR T_ELAPSED_MS "(${T_END_MS} - ${T_START_MS})" )
    message( STATUS "Generating ${UMBRELLA_HEADER_FILE} umbrella header done (${T_ELAPSED_MS}ms)" )
    message( STATUS "Umbrella header has been written to: ${OUTPUT_FILE_PATH}" )
    #
endfunction()