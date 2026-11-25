package io.github.lucaaszsx.betteraethersx2.input

// The inputs of an emulated DualShock 2. The order matches PadDualshock2::Inputs in upstream
// pcsx2/SIO/Pad/PadDualshock2.h, because the ordinal is the index the core expects.
enum class PadBind {
    UP,
    RIGHT,
    DOWN,
    LEFT,
    TRIANGLE,
    CIRCLE,
    CROSS,
    SQUARE,
    SELECT,
    START,
    L1,
    L2,
    R1,
    R2,
    L3,
    R3,
    ANALOG,
    PRESSURE,
    LEFT_STICK_UP,
    LEFT_STICK_RIGHT,
    LEFT_STICK_DOWN,
    LEFT_STICK_LEFT,
    RIGHT_STICK_UP,
    RIGHT_STICK_RIGHT,
    RIGHT_STICK_DOWN,
    RIGHT_STICK_LEFT,
}
