package io.github.lucaaszsx.betteraethersx2.runtime

import java.io.File

// The real emulator core in libbax.so.
class NativeRuntime : EmulatorRuntime {
    override val coreVersion: String = nativeCoreVersion()

    override fun identifyBios(file: File): BiosIdentity? {
        val (region, version) = nativeIdentifyBios(file.absolutePath) ?: return null
        return BiosIdentity(region, version)
    }

    private companion object {
        init {
            System.loadLibrary("bax")
        }

        @JvmStatic
        external fun nativeCoreVersion(): String

        // Returns [region, version], or null when the file is not a BIOS.
        @JvmStatic
        external fun nativeIdentifyBios(path: String): Array<String>?
    }
}
