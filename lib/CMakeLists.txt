# # "cmake-lists": ash, "author": alessandromanzini
# Ash lib specific logic here.
#
# ========================================
# TARGET CONFIGURATION
# ========================================
#
cdrk_module( ${PROJECT_NAME} LIBRARY
             INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/import"
             SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source"
             STRICT_ARGS ${ASH_COMPILER_SETTINGS} )
#
# Glob all project files:
# 1. Module interface units.
cdrk_module_sources( "${MODULE_TARGET}" MODULES "*.cppm" )
#
# 2. Shared source files.
# cdrk_module_sources( "${MODULE_TARGET}" SOURCES "${MODULE_SRC_DIR}/shared/*.cpp" )
#
# 3a. Apple Obj-C. We also disable pedantic on these files cause they behave differently.
cdrk_module_sources( "${MODULE_TARGET}"
                     FRAMEWORKS "Cocoa"
                     SOURCES "platform/darwin/*.mm"
                     NO_PEDANTIC
                     IF APPLE )
#
# 3b. Apple source files.3
cdrk_module_sources( "${MODULE_TARGET}"
                     SOURCES "platform/darwin/*.cpp"
                     IF APPLE )
#
# 4. Win32 source files.
cdrk_module_sources( "${MODULE_TARGET}"
                     SOURCES "platform/win32/*.cpp"
                     IF WIN32 )
#
#
# ========================================
# EXTERNAL DEPENDENCIES
# ========================================
#
# bhop, linked statically. Linked to the object target so both the static and shared ash libraries get it, and PUBLIC because ash's exported
# module interfaces import bhop, so consumers need its BMIs too.
target_link_libraries( "${MODULE_TARGET}-object" PUBLIC bhop::bhop-static )
#
# GLM (math library)
#include( fetch_glm_macro )
#fetch_glm()
#
# SDL2 (graphics/input)
#include( fetch_sdl2_macro )
#fetch_sdl2()
