package io.github.lucaaszsx.betteraethersx2.runtime

// The UI talks to the emulator only through this interface, so screens can be built and tested against
// FakeRuntime while the native runtime is still being written.
interface EmulatorRuntime {
    val coreVersion: String
}
