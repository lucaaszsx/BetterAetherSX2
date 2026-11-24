package io.github.lucaaszsx.betteraethersx2.runtime

// The real emulator core in libbax.so.
class NativeRuntime : EmulatorRuntime {
    override val coreVersion: String = nativeCoreVersion()

    private companion object {
        init {
            System.loadLibrary("bax")
        }

        @JvmStatic
        external fun nativeCoreVersion(): String
    }
}
