# "cmake-lists": ash, "author": alessandromanzini
# Top-level cmake file, includes the lib and test subdirectories.
#
cmake_minimum_required( VERSION 4.0 )
#
# Gate for `import std` — must be set before project(). UUID is CMake-version specific (4.3.x).
set( CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" )
#
# Homebrew's clang ships libc++.modules.json under <prefix>/lib/c++, which `clang -print-file-name`
# fails to resolve, so CMake's auto-discovery breaks. Point CMake at it directly when we can find it.
# Must run before project() (toolchain detection). Only overrides when the file actually exists, so
# it is a no-op for toolchains whose auto-discovery already works.
if( NOT CMAKE_CXX_STDLIB_MODULES_JSON )
    # The compiler may be given as a bare name (e.g. a preset with "clang++"), so resolve it to a full
    # path via PATH / $ENV{CXX} before deriving the sibling lib/c++ directory.
    set( _ash_cxx "${CMAKE_CXX_COMPILER}" )
    if( NOT _ash_cxx )
        set( _ash_cxx "$ENV{CXX}" )
    endif()
    if( NOT _ash_cxx )
        set( _ash_cxx "c++" )
    endif()
    find_program( _ash_cxx_path NAMES "${_ash_cxx}" )
    if( _ash_cxx_path )
        get_filename_component( _ash_bindir "${_ash_cxx_path}" DIRECTORY )
        get_filename_component( _ash_modules_json "${_ash_bindir}/../lib/c++/libc++.modules.json" ABSOLUTE )
        if( EXISTS "${_ash_modules_json}" )
            set( CMAKE_CXX_STDLIB_MODULES_JSON "${_ash_modules_json}" )
        endif()
    endif()
    unset( _ash_cxx )
    unset( _ash_cxx_path CACHE )
    unset( _ash_bindir )
    unset( _ash_modules_json )
endif()
#
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
option( ASH_BUILD_TESTS "Build test executable" ON )
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
set( CXX_CLANG_TIDY clang-tidy )
#
# `import std` support is enabled via CMAKE_CXX_MODULE_STD + the version-gated experimental UUID.
# The UUID must be set before project() (done at the top of this file); CMAKE_CXX_MODULE_STD is read
# when targets are created, so setting it here (before add_subdirectory) is fine.
set( CMAKE_CXX_MODULE_STD ON )
message( STATUS "import std support enabled." )
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
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti" )
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
if( ${ASH_BUILD_TESTS} AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test" )
    #
    add_subdirectory( "test" )
    #
endif()
