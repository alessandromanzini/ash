# "cmake-lists": ash, "author": alessandromanzini
# Top-level cmake file, includes the lib and test subdirectories.
#
cmake_minimum_required( VERSION 4.0 )
project( ash
         VERSION 0.1.0
         DESCRIPTION "A modern C++ error handling library."
         LANGUAGES CXX )
#
#
# ========================================
# CMAKE MODULES
# ========================================
#
# Append cmake module path, making sure cmake finds our custom modules. (.cmake)
list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
#
include( cdrake )
#
#
# ========================================
# PROJECT OPTIONS
# ========================================
#
option( ASH_ENABLE_EXCEPTIONS "Enable exceptions for ash target." OFF )
option( ASH_ENABLE_RTTI "Enable rtti for ash target." OFF )
#
# option( ASH_IMPORT_UNIT_TEST "Import unit test target." ON )
#
# ========================================
# GLOBAL CONFIGURATION
# ========================================
#
message( STATUS "Configuring project: ${PROJECT_NAME}" )
#
# C++ standard configuration.
set( CMAKE_CXX_STANDARD 26 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
message( STATUS "C++${CMAKE_CXX_STANDARD} support has been enabled by default." )
#
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
set( CXX_CLANG_TIDY "clang-tidy" )
#
# Build type configuration.
if( NOT CMAKE_BUILD_TYPE )
    #
    set( CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE )
    set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo" )
    #
endif()
message( STATUS "Build Type: ${CMAKE_BUILD_TYPE}" )
#
#
# ========================================
# COMPILER-SPECIFIC SETTINGS
# ========================================
#
if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
    #
    set( CBRIDGE_SRC "clang_cbridge.cpp" )
    #
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    #
    set( CBRIDGE_SRC "gnu_cbridge.cpp" )
    #
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    #
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP" ) # Enable parallel compilation on MSVC.
    set( CBRIDGE_SRC "msvc_cbridge.cpp" )
    #
else()
    #
    message( FATAL_ERROR "Unsupported compiler... COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\"" )
    #
endif()
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
# Set output directories for all build configurations.
foreach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )
    #
    string( TOUPPER ${OUTPUT_CONFIG} OUTPUT_CONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/bin" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/lib" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} "${CMAKE_BINARY_DIR}/lib" )
    #
endforeach()
#
#
# ========================================
# ADD SUBDIRECTORIES
# ========================================
#
# Core library.
add_subdirectory( "lib" )
#
# Test executable.
if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/local" )
    #
    add_subdirectory( "local" )
    #
endif()
