package io.github.lucaaszsx.betteraethersx2.runtime

import android.view.Surface
import java.io.File

// The UI talks to the emulator only through this interface, so screens can be built and tested against
// FakeRuntime while the native runtime is still being written.
interface EmulatorRuntime {
    val coreVersion: String

    // Null when the file is not a PS2 BIOS the core can boot.
    fun identifyBios(file: File): BiosIdentity?

    // Points the core at its folders and loads its settings. False when its data files are missing.
    fun initialize(
        dataDirectory: File,
        resourcesDirectory: File,
    ): Boolean

    // Boots the BIOS to its menu, drawing into the surface.
    fun startSession(surface: Surface)

    fun resizeSurface(
        width: Int,
        height: Int,
    )

    // Stops the session; the surface may be released afterwards.
    fun stopSession()
}

data class BiosIdentity(
    val region: String,
    val version: String,
)
