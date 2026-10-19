# # "cmake-lists": ash, "author": alessandromanzini
# Ash lib specific logic here.
#
# ========================================
# TARGET CONFIGURATION
# ========================================
#
# todo: ASH_ENABLE_EXCEPTIONS and ASH_ENABLE_RTTI
cdrk_module( ${PROJECT_NAME} LIBRARY
             INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/import"
             SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source"
             STRICT_ARGS "$<$<CONFIG:Debug>:-g3>" )
#
# Glob all project files:
# 1. Module interface units.
cdrk_module_sources( "${MODULE_TARGET}" MODULES "*.cppm" )
#
# 2. Shared source files.
# cdrk_module_sources( "${MODULE_TARGET}" SOURCES "${MODULE_SRC_DIR}/shared/*.cpp" )
#
# 3. Apple Obj-C. We also disable pedantic on these files cause they behave differently.
cdrk_module_sources( "${MODULE_TARGET}"
                     FRAMEWORKS "Cocoa"
                     SOURCES "platform/darwin/*.mm"
                     NO_PEDANTIC
                     IF APPLE )
#
# 4. Win32 source files.
cdrk_module_sources( "${MODULE_TARGET}"
                     SOURCES "platform/win32/*.cpp"
                     IF WIN32 )
#
# 5. CBridge headers.
cdrk_module_sources( "${MODULE_TARGET}"
                     SOURCES "platform/compiler/${CBRIDGE_SRC_FILENAME}"
                     NO_PEDANTIC )
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
