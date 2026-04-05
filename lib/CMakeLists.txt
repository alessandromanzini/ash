# # "cmake-lists": mongoose, "author": alessandromanzini
# mongoose lib specific logic here.
#
#
# ========================================
# CMAKE MODULES
# ========================================
#
include(add_library_distribution_targets)
include(generate_umbrella_header)
include(mark_non_standard_sources)
include(target_compile_options_strict)
#
#
# ========================================
# TARGET CONFIGURATION
# ========================================
#
set( OBJECT_TARGET "${PROJECT_NAME}_object" )
set( STATIC_TARGET "${PROJECT_NAME}_static" )
set( SHARED_TARGET "${PROJECT_NAME}_shared" )
#
set( SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" )
set( INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" )
#
# the OBJECT lib target serves as the single compile source for the next static/shared libs.
# we compile the objs ones to the allow users to link to either without extra overhead.
add_library( ${OBJECT_TARGET} OBJECT )
#
# glob shared and platform source and append them to the OBJECT target.
file( GLOB_RECURSE TARGET_SOURCES "${SRC_DIR}/shared/*.cpp" )
#
if( APPLE )
    target_link_libraries( ${OBJECT_TARGET} PRIVATE "-framework Cocoa" )
    file( GLOB_RECURSE APPLE_OBJ_C_FILES "${SRC_DIR}/platform/darwin/*.mm" )
    mark_non_standard_sources( ${APPLE_OBJ_C_FILES} )
    list( APPEND TARGET_SOURCES ${APPLE_OBJ_C_FILES} )
elseif( WIN32 )
    file( GLOB_RECURSE WIN32_CPP_FILES "${SRC_DIR}/platform/win32/*.cpp" )
    list( APPEND TARGET_SOURCES ${WIN32_CPP_FILES} )
endif()
#
list( APPEND TARGET_SOURCES ${CBRIDGE_SRC} )
#
target_sources( ${OBJECT_TARGET} PRIVATE ${TARGET_SOURCES} )
#
# set OBJECT target compile options and directories.
target_compile_options_strict( ${OBJECT_TARGET} )
target_compile_options( ${OBJECT_TARGET} PRIVATE -g3 )
#
target_include_directories( ${OBJECT_TARGET}
                            PUBLIC $<BUILD_INTERFACE:${INC_DIR}> $<INSTALL_INTERFACE:include>
                            PRIVATE ${SRC_DIR} )
#
# set OBJECT target precompile headers
target_precompile_headers( ${OBJECT_TARGET} PRIVATE "${INC_DIR}/${PROJECT_NAME}/pch.hpp" )
#
# finally add distribution targets
add_library_distribution_targets( ${STATIC_TARGET} ${SHARED_TARGET} ${OBJECT_TARGET} ${PROJECT_NAME} )
#
#
# ========================================
# UMBRELLA HEADER
# ========================================
generate_umbrella_header(
        UMBRELLA_HEADER_NAME ${PROJECT_NAME}
        INCLUDE_DIRECTORY ${INC_DIR}
        OUTPUT_DIRECTORY "${INC_DIR}/${PROJECT_NAME}"
        EXCEPTION_REGEX "cbridge.hpp" )
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
