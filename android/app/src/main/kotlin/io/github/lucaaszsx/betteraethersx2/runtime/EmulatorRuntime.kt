package io.github.lucaaszsx.betteraethersx2.runtime

import android.app.Activity
import android.view.Surface
import io.github.lucaaszsx.betteraethersx2.input.PadBind
import io.github.lucaaszsx.betteraethersx2.input.TouchAction
import java.io.File

// The UI talks to the emulator only through this interface, so screens can be built and tested against
// FakeRuntime while the native runtime is still being written.
interface EmulatorRuntime {
    val coreVersion: String

    // Null when the file is not a PS2 BIOS the core can boot.
    fun identifyBios(file: File): BiosIdentity?

    // Reads a disc image's serial and looks up its title. Null when the file is not a PS2 disc image, or while a
    // session runs, since the core reads discs through one shared interface.
    fun identifyGame(uri: String): GameIdentity?

    // Points the core at its folders and loads its settings. False when its data files are missing.
    fun initialize(
        dataDirectory: File,
        resourcesDirectory: File,
    ): Boolean

    // Boots the game, a disc image URI, or the BIOS alone when it is null, drawing into the surface. The listener hears
    // once when the session ends. The host is the activity showing the game, which the audio output needs until the
    // session stops.
    fun startSession(
        host: Activity,
        surface: Surface,
        game: String?,
        listener: SessionListener,
    )

    // Also lays out the on-screen controls for the new size; density is pixels per dp.
    fun resizeSurface(
        width: Int,
        height: Int,
        density: Float,
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

    // Sets one input of the first controller: 0 is released, 1 is fully pressed or fully tilted. Hides the
    // on-screen controls until the next touch.
    fun setPadInput(
        bind: PadBind,
        value: Float,
    )

    // A finger on the game surface, in surface pixels. The core decides which on-screen control it presses.
    fun touch(
        action: TouchAction,
        pointerId: Int,
        x: Float,
        y: Float,
    )
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

data class GameIdentity(
    val serial: String,
    val title: String,
    val region: String,
)

data class BiosIdentity(
    val region: String,
    val version: String,
)
