# The app's native library. Configured as an external frontend of the PCSX2 build (PCSX2_EXTERNAL_FRONTEND_DIR),
# so it links the core targets directly.

get_filename_component(BAX_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

set(BAX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${BAX_REPO_ROOT}/core" "${CMAKE_BINARY_DIR}/bax-core")

# The host callbacks PCSX2 expects from a frontend, shared by the app library and the device-side test tools.
add_library(bax_host OBJECT
    AndroidHost.cpp
    ContentUris.cpp
    NativeRuntime.cpp
    Session.cpp
    TouchControls.cpp
)

set_target_properties(bax_host PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(bax_host PUBLIC
    "${CMAKE_BINARY_DIR}/common/include"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/pcsx2"
)

target_link_libraries(bax_host PUBLIC
    PCSX2_FLAGS
    PCSX2
    bax_core
    android
    log
)

add_library(bax SHARED)
target_link_libraries(bax PRIVATE bax_host)

add_subdirectory("${BAX_REPO_ROOT}/core/difftest" "${CMAKE_BINARY_DIR}/bax-difftest")
