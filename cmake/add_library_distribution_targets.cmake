function( add_library_distribution_targets STATIC_TARGET SHARED_TARGET OBJECT_TARGET DIST_NAME )
    #
    # add static/shared library targets
    add_library( ${STATIC_TARGET} STATIC )
    add_library( ${SHARED_TARGET} SHARED )
    #
    # namespaced aliases for find_package / FetchContent consumers
    add_library( ${DIST_NAME}::${DIST_NAME}-static ALIAS ${STATIC_TARGET} )
    add_library( ${DIST_NAME}::${DIST_NAME} ALIAS ${SHARED_TARGET} )
    #
    # static and shared targets link against object
    target_link_libraries( ${STATIC_TARGET} PUBLIC ${OBJECT_TARGET} )
    set_target_properties( ${STATIC_TARGET} PROPERTIES OUTPUT_NAME ${DIST_NAME} )
    #
    target_link_libraries( ${SHARED_TARGET} PUBLIC ${OBJECT_TARGET} )
    set_target_properties( ${SHARED_TARGET} PROPERTIES OUTPUT_NAME ${DIST_NAME} )
    #
    # add compile macros for dynamic symbols exports
    string( TOUPPER ${DIST_NAME} UPPER_DIST_NAME )
    target_compile_definitions( ${STATIC_TARGET} PUBLIC ${UPPER_DIST_NAME}_STATIC )
    target_compile_definitions( ${SHARED_TARGET}
                                PUBLIC ${UPPER_DIST_NAME}_SHARED
                                PRIVATE ${UPPER_DIST_NAME}_BUILD_LIB )
    #
endfunction()