# Our AArch64 EE recompiler. It works on PCSX2's CPU state, so it builds only inside the PCSX2 build for ARM64, added
# by the Android app's native library and by the Linux differential tester.
add_library(bax_jit OBJECT
    block_links.cpp
    ee/ee_alu.cpp
    ee/ee_block.cpp
    ee/ee_branch.cpp
    ee/ee_memory.cpp
    ee/ee_rec.cpp
)

set_target_properties(bax_jit PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(bax_jit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(bax_jit PRIVATE
    "${CMAKE_BINARY_DIR}/common/include"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pcsx2"
)

target_compile_definitions(bax_jit PUBLIC BAX_EE_REC)
target_link_libraries(bax_jit PUBLIC PCSX2_FLAGS PCSX2 bax_core vixl)

# VMManager selects the recompiler only in a core built with it.
target_compile_definitions(PCSX2 PRIVATE BAX_EE_REC)
