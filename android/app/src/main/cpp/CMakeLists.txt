# The app's native library. Configured as an external frontend of the PCSX2 build (PCSX2_EXTERNAL_FRONTEND_DIR),
# so it links the core targets directly.

get_filename_component(BAX_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

set(BAX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${BAX_REPO_ROOT}/core" "${CMAKE_BINARY_DIR}/bax-core")

add_library(bax SHARED
    AndroidHost.cpp
    NativeRuntime.cpp
    Session.cpp
)

target_include_directories(bax PRIVATE
    "${CMAKE_BINARY_DIR}/common/include"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pcsx2"
)

target_link_libraries(bax PRIVATE
    PCSX2_FLAGS
    PCSX2
    bax_core
    android
    log
)
