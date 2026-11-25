package io.github.lucaaszsx.betteraethersx2.runtime

import android.app.Activity
import android.view.Surface
import io.github.lucaaszsx.betteraethersx2.input.PadBind
import io.github.lucaaszsx.betteraethersx2.input.TouchAction
import org.libsdl.app.SDL
import java.io.File

// The real emulator core in libbax.so. Its state is process-wide, so the app keeps a single instance.
class NativeRuntime : EmulatorRuntime {
    override val coreVersion: String = nativeCoreVersion()

    override fun identifyBios(file: File): BiosIdentity? {
        val (region, version) = nativeIdentifyBios(file.absolutePath) ?: return null
        return BiosIdentity(region, version)
    }

    override fun identifyGame(uri: String): GameIdentity? {
        val (serial, title, region) = nativeIdentifyGame(uri) ?: return null
        return GameIdentity(serial, title, region)
    }

    override fun initialize(
        dataDirectory: File,
        resourcesDirectory: File,
    ): Boolean = nativeInitialize(dataDirectory.absolutePath, resourcesDirectory.absolutePath)

    override fun startSession(
        host: Activity,
        surface: Surface,
        game: String?,
        listener: SessionListener,
    ) {
        // SDL plays the sound and asks Android for the audio devices through this activity.
        SDL.setContext(host)
        nativeStartSession(surface, game, listener)
    }

    override fun resizeSurface(
        width: Int,
        height: Int,
        density: Float,
    ) {
        nativeResizeSurface(width, height)
        nativeSetTouchLayout(width, height, density)
    }

    override fun stopSession() {
        nativeStopSession()
        SDL.setContext(null)
    }

    override fun setPaused(paused: Boolean) = nativeSetPaused(paused)

    override fun attachSurface(surface: Surface) = nativeAttachSurface(surface)

    override fun detachSurface() = nativeDetachSurface()

    override fun setPadInput(
        bind: PadBind,
        value: Float,
    ) = nativeSetPadInput(bind.ordinal, value)

    override fun touch(
        action: TouchAction,
        pointerId: Int,
        x: Float,
        y: Float,
    ) = nativeTouch(action.ordinal, pointerId, x, y)

    // One external function per JNI entry point in libbax.so; splitting them would only scatter the native surface.
    @Suppress("TooManyFunctions")
    private companion object {
        init {
            System.loadLibrary("bax")
            // Loading libbax.so loads libSDL3.so, which registers SDL's Java natives. SDL's audio code calls back
            // into those classes, so they are set up once here, as SDLActivity would do.
            SDL.setupJNI()
            SDL.initialize()
        }

        @JvmStatic
        external fun nativeCoreVersion(): String

        // Returns [region, version], or null when the file is not a BIOS.
        @JvmStatic
        external fun nativeIdentifyBios(path: String): Array<String>?

        // Returns [serial, title, region], or null when the file is not a PS2 disc image.
        @JvmStatic
        external fun nativeIdentifyGame(uri: String): Array<String>?

        @JvmStatic
        external fun nativeInitialize(
            dataDirectory: String,
            resourcesDirectory: String,
        ): Boolean

        @JvmStatic
        external fun nativeStartSession(
            surface: Surface,
            game: String?,
            listener: SessionListener,
        )

        @JvmStatic
        external fun nativeResizeSurface(
            width: Int,
            height: Int,
        )

        @JvmStatic
        external fun nativeStopSession()

        @JvmStatic
        external fun nativeSetPaused(paused: Boolean)

        @JvmStatic
        external fun nativeAttachSurface(surface: Surface)

        @JvmStatic
        external fun nativeDetachSurface()

        @JvmStatic
        external fun nativeSetPadInput(
            bind: Int,
            value: Float,
        )

        @JvmStatic
        external fun nativeSetTouchLayout(
            width: Int,
            height: Int,
            density: Float,
        )

        @JvmStatic
        external fun nativeTouch(
            action: Int,
            pointerId: Int,
            x: Float,
            y: Float,
        )
    }
}
