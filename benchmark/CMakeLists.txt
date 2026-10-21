# # "cmake-lists": benchmark, "author": alessandromanzini
# Ash benchmark profiles here.
#
include( cdrake )

cdrk_module( codec_bench EXECUTABLE )
cdrk_module_sources( ${MODULE_TARGET} SOURCES "codec_bench.cpp" )
target_link_libraries( ${MODULE_TARGET} PRIVATE ash::ash )
