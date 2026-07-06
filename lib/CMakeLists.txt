# # "cmake-lists": ash, "author": alessandromanzini
# Ash lib specific logic here.
#
# ========================================
# TARGET CONFIGURATION
# ========================================
#
set( OBJECT_TARGET "${PROJECT_NAME}_object" )
set( STATIC_TARGET "${PROJECT_NAME}_static" )
set( SHARED_TARGET "${PROJECT_NAME}_shared" )
#
set( INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/import" )
set( SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source" )
#
# The OBJECT lib target serves as the single compile source for the next static/shared libs.
# We compile the objs ones to the allow users to link to either without extra overhead.
add_library( ${OBJECT_TARGET} OBJECT )
#
# Glob module interface files and add them to the OBJECT target.
file( GLOB_RECURSE MODULE_INTERFACE_FILES "${INC_DIR}/*.cppm" )
target_sources( ${OBJECT_TARGET}
                PUBLIC
                FILE_SET export_modules
                TYPE CXX_MODULES
                FILES ${MODULE_INTERFACE_FILES} )
#
# Glob shared and platform source and append them to the OBJECT target.
file( GLOB_RECURSE TARGET_SOURCES "${SRC_DIR}/shared/*.cpp" )
#
if( APPLE )
    #
    target_link_libraries( ${OBJECT_TARGET} PRIVATE "-framework Cocoa" )
    file( GLOB_RECURSE APPLE_OBJ_C_FILES "${SRC_DIR}/platform/darwin/*.mm" )
    cdrk_sources_disable_pedantic( ${APPLE_OBJ_C_FILES} )
    list( APPEND TARGET_SOURCES ${APPLE_OBJ_C_FILES} )
    #
elseif( WIN32 )
    #
    file( GLOB_RECURSE WIN32_CPP_FILES "${SRC_DIR}/platform/win32/*.cpp" )
    list( APPEND TARGET_SOURCES ${WIN32_CPP_FILES} )
    #
endif()
#
set( CBRIDGE_ABS_PATH "${SRC_DIR}/platform/compiler/${CBRIDGE_SRC}" )
cdrk_sources_disable_pedantic( ${CBRIDGE_ABS_PATH} )
list( APPEND TARGET_SOURCES ${CBRIDGE_ABS_PATH} )
#
# Set OBJECT target precompile headers.
# target_precompile_headers( ${OBJECT_TARGET} PRIVATE "${INC_DIR}/${PROJECT_NAME}/pch.hpp" )
#
target_sources( ${OBJECT_TARGET} PRIVATE ${TARGET_SOURCES} )
#
# Set OBJECT target compile options and directories.
# We must keep exceptions and rtti enabled because the public compile options must match for BMIs.
cdrk_target_configure_strict( ${OBJECT_TARGET}
                              ENABLE_EXCEPTIONS ${ASH_ENABLE_EXCEPTIONS}
                              ENABLE_RTTI ${ASH_ENABLE_RTTI}
                              $<$<CONFIG:Debug>:-g3> )
#
target_include_directories( ${OBJECT_TARGET}
                            PUBLIC $<BUILD_INTERFACE:${INC_DIR}> $<INSTALL_INTERFACE:include>
                            PRIVATE ${SRC_DIR} )
#
# Finally add distribution targets.
cdrk_add_library_dist_targets( ${STATIC_TARGET} ${SHARED_TARGET} ${OBJECT_TARGET} ${PROJECT_NAME} )
#
#
# ========================================
# EXTERNAL DEPENDENCIES
# ========================================
#
# GLM (math library)
#include( fetch_glm_macro )
#fetch_glm()
#
# SDL2 (graphics/input)
#include( fetch_sdl2_macro )
#fetch_sdl2()
