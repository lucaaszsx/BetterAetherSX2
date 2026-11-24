package io.github.lucaaszsx.betteraethersx2.runtime

import java.io.File

// The UI talks to the emulator only through this interface, so screens can be built and tested against
// FakeRuntime while the native runtime is still being written.
interface EmulatorRuntime {
    val coreVersion: String

    // Null when the file is not a PS2 BIOS the core can boot.
    fun identifyBios(file: File): BiosIdentity?
}

data class BiosIdentity(
    val region: String,
    val version: String,
)
