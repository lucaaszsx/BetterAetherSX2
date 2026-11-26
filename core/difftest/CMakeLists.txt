# The EE differential tester. It links the PCSX2 core and the app's host callbacks, so it is built only inside the
# PCSX2 build for Android (android/app/src/main/cpp) and runs on a device through adb.
add_executable(bax_difftest
    ee_cases.cpp
    ee_state.cpp
    main.cpp
)

target_link_libraries(bax_difftest PRIVATE
    bax_host
    PCSX2_FLAGS
    PCSX2
)
