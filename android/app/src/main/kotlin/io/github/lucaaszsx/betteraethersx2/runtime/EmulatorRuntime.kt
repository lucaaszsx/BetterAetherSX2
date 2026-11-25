package io.github.lucaaszsx.betteraethersx2.runtime

import android.app.Activity
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

    // Boots the BIOS to its menu, drawing into the surface. The listener hears once when the session ends. The host is
    // the activity showing the game, which the audio output needs until the session stops.
    fun startSession(
        host: Activity,
        surface: Surface,
        listener: SessionListener,
    )

    fun resizeSurface(
        width: Int,
        height: Int,
    )

    // Stops the session; the surface may be released afterwards.
    fun stopSession()

    // Freezes or resumes emulation and sound; the session keeps its state.
    fun setPaused(paused: Boolean)

    // Gives a running session a new surface after detachSurface.
    fun attachSurface(surface: Surface)

    // Takes the surface away from a running session and returns once the core no longer uses it, so the surface may
    // be released afterwards. The session keeps running without drawing.
    fun detachSurface()
}

// Called on the emulator's own thread, so implementations hand the news to the UI thread and return.
fun interface SessionListener {
    // failed is false only when the app asked the session to stop. reason is the core's explanation, in English,
    // and empty when the core gave none.
    fun onSessionEnded(
        failed: Boolean,
        reason: String,
    )
}

data class BiosIdentity(
    val region: String,
    val version: String,
)
