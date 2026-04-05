# "cmake-lists": mongoose, "author": alessandromanzini
# top-level cmake file, includes the lib and test subdirectories.
#
cmake_minimum_required( VERSION 3.29 )
project( mongoose
         VERSION 0.1.0
         DESCRIPTION "A modern C++ error handling library."
         LANGUAGES CXX )
#
#
# ========================================
# CMAKE MODULES
# ========================================
#
# append cmake module path, making sure cmake finds our custom modules (.cmake)
list( APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" )
#
include(mark_non_standard_sources)
#
#
# ========================================
# GLOBAL CONFIGURATION
# ========================================
#
# c++ standard configuration
set( CMAKE_CXX_STANDARD 26 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )
message( STATUS "C++${CMAKE_CXX_STANDARD} support has been enabled by default." )
#
# enable experimental module support
# set( CMAKE_CXX_MODULE_STD ON )
# set( CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508" )
# message( STATUS "Module support has been enabled." )
#
# build type configuration
if( NOT CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE )
    set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo" )
endif()
message( STATUS "Build Type: ${CMAKE_BUILD_TYPE}" )
#
#
# ========================================
# COMPILER-SPECIFIC SETTINGS
# ========================================
#
set( CBRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/lib/src/platform/compiler/" )
if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library" )
    string( APPEND CBRIDGE_SRC "clang_cbridge.cpp" )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    string( APPEND CBRIDGE_SRC "gnu_cbridge.cpp" )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP" ) # enable parallel compilation on MSVC
    string( APPEND CBRIDGE_SRC "msvc_cbridge.cpp" )
else()
    message( FATAL_ERROR "Unsupported compiler... COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\"" )
endif()
mark_non_standard_sources( ${CBRIDGE_SRC} )
message( STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler configuration." )
#
#
# ========================================
# OUTPUT DIRECTORIES
# ========================================
#
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" )
#
# set output directories for all build configurations
foreach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUT_CONFIG} OUTPUT_CONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/bin" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/lib" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/lib" )
endforeach()
#
#
# ========================================
# PROJECT OPTIONS
# ========================================
#
option( MONGOOSE_BUILD_TESTS "Build test executable" ON )
#
#
# ========================================
# ADD SUBDIRECTORIES
# ========================================
#
# core library
add_subdirectory( "lib" )
#
# test executable
if( ${MONGOOSE_BUILD_TESTS} )
    add_subdirectory( "test" )
endif()
