package io.github.lucaaszsx.betteraethersx2.runtime

import android.view.Surface
import java.io.File

// The real emulator core in libbax.so. Its state is process-wide, so the app keeps a single instance.
class NativeRuntime : EmulatorRuntime {
    override val coreVersion: String = nativeCoreVersion()

    override fun identifyBios(file: File): BiosIdentity? {
        val (region, version) = nativeIdentifyBios(file.absolutePath) ?: return null
        return BiosIdentity(region, version)
    }

    override fun initialize(
        dataDirectory: File,
        resourcesDirectory: File,
    ): Boolean = nativeInitialize(dataDirectory.absolutePath, resourcesDirectory.absolutePath)

    override fun startSession(surface: Surface) = nativeStartSession(surface)

    override fun resizeSurface(
        width: Int,
        height: Int,
    ) = nativeResizeSurface(width, height)

    override fun stopSession() = nativeStopSession()

    private companion object {
        init {
            System.loadLibrary("bax")
        }

        @JvmStatic
        external fun nativeCoreVersion(): String

        // Returns [region, version], or null when the file is not a BIOS.
        @JvmStatic
        external fun nativeIdentifyBios(path: String): Array<String>?

        @JvmStatic
        external fun nativeInitialize(
            dataDirectory: String,
            resourcesDirectory: String,
        ): Boolean

        @JvmStatic
        external fun nativeStartSession(surface: Surface)

        @JvmStatic
        external fun nativeResizeSurface(
            width: Int,
            height: Int,
        )

        @JvmStatic
        external fun nativeStopSession()
    }
}
