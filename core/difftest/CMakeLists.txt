# The EE differential tester. It links the PCSX2 core, so it builds only inside the PCSX2 build: for Android through
# the app's frontend (android/app/src/main/cpp), run on a device with tools/difftest/run.sh, and for Linux AArch64 as
# the frontend itself (PCSX2_EXTERNAL_FRONTEND_DIR pointing here), run by CI on an arm64 runner.
if(NOT ANDROID)
    # As the frontend itself, the tester adds what the app's frontend adds on Android: our core and the recompiler.
    get_filename_component(BAX_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(BAX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BAX_REPO_ROOT}/core" "${CMAKE_BINARY_DIR}/bax-core")
    add_subdirectory("${BAX_REPO_ROOT}/core/jit" "${CMAKE_BINARY_DIR}/bax-jit")
endif()

add_executable(bax_difftest
    ee_cases.cpp
    ee_state.cpp
    main.cpp
)

target_link_libraries(bax_difftest PRIVATE
    bax_jit
    PCSX2_FLAGS
    PCSX2
)

if(ANDROID)
    target_link_libraries(bax_difftest PRIVATE bax_host)
else()
    # The app's host callbacks are Android code; upstream's test stub answers the core's host calls instead.
    target_sources(bax_difftest PRIVATE "${CMAKE_SOURCE_DIR}/tests/ctest/core/StubHost.cpp")
endif()
