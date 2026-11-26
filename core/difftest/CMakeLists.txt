# The EE differential tester. It links the PCSX2 core, so it builds only inside the PCSX2 build: for Android through
# the app's frontend (android/app/src/main/cpp), run on a device with tools/difftest/run.sh, and for Linux AArch64 as
# the frontend itself (PCSX2_EXTERNAL_FRONTEND_DIR pointing here), run by CI on an arm64 runner.
add_executable(bax_difftest
    ee_cases.cpp
    ee_state.cpp
    main.cpp
)

target_link_libraries(bax_difftest PRIVATE
    PCSX2_FLAGS
    PCSX2
)

if(ANDROID)
    target_link_libraries(bax_difftest PRIVATE bax_host)
else()
    # The app's host callbacks are Android code; upstream's test stub answers the core's host calls instead.
    target_sources(bax_difftest PRIVATE "${CMAKE_SOURCE_DIR}/tests/ctest/core/StubHost.cpp")
endif()
