package io.github.lucaaszsx.betteraethersx2.runtime

import android.app.Activity
import android.view.Surface
import io.github.lucaaszsx.betteraethersx2.input.PadBind
import io.github.lucaaszsx.betteraethersx2.input.TouchAction
import java.io.File

// Stands in for the native core in UI work and tests. It never emulates anything.
class FakeRuntime(
    private val bios: BiosIdentity? = BiosIdentity(region = "USA", version = "2.20"),
    private val game: GameIdentity? = GameIdentity(serial = "SLUS-00000", title = "Fake Game", region = "NTSC-U"),
) : EmulatorRuntime {
    private var listener: SessionListener? = null

    override val coreVersion: String = "0.0.0-fake"

    override fun identifyBios(file: File): BiosIdentity? = bios

    override fun identifyGame(uri: String): GameIdentity? = game

    override fun initialize(
        dataDirectory: File,
        resourcesDirectory: File,
    ): Boolean = true

    override fun startSession(
        host: Activity,
        surface: Surface,
        listener: SessionListener,
    ) {
        this.listener = listener
    }

    override fun resizeSurface(
        width: Int,
        height: Int,
        density: Float,
    ) = Unit

    override fun stopSession() {
        listener?.onSessionEnded(false, "")
        listener = null
    }

    override fun setPaused(paused: Boolean) = Unit

    override fun attachSurface(surface: Surface) = Unit

    override fun detachSurface() = Unit

    override fun setPadInput(
        bind: PadBind,
        value: Float,
    ) = Unit

    override fun touch(
        action: TouchAction,
        pointerId: Int,
        x: Float,
        y: Float,
    ) = Unit
}
