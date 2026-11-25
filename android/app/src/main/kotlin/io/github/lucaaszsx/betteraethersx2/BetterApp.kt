package io.github.lucaaszsx.betteraethersx2

import android.app.Application
import android.content.res.AssetManager
import io.github.lucaaszsx.betteraethersx2.runtime.CoreResources
import io.github.lucaaszsx.betteraethersx2.runtime.EmulatorRuntime
import io.github.lucaaszsx.betteraethersx2.runtime.NativeRuntime
import io.github.lucaaszsx.betteraethersx2.runtime.ResourceSource
import java.io.InputStream

// Owns the one emulator runtime of the process, shared by every screen.
class BetterApp : Application() {
    val runtime: EmulatorRuntime by lazy { NativeRuntime() }

    // The core initializes once per process and aborts if asked again. A new library screen in the same process,
    // for example after returning from the launcher while a game runs, gets the first result.
    private val runtimeReady: Boolean by lazy {
        val installStamp = packageManager.getPackageInfo(packageName, 0).lastUpdateTime
        val resources = filesDir.resolve("resources")
        CoreResources(AssetSource(assets), resources).install(installStamp)
        runtime.initialize(filesDir, resources)
    }

    // Extracts the core's resources if this install has not done so yet, then initializes the core.
    // Runs on a background thread: the first extraction copies several megabytes.
    fun prepareRuntime(): Boolean = runtimeReady

    private class AssetSource(
        private val assets: AssetManager,
    ) : ResourceSource {
        override fun list(path: String): Array<String> = assets.list(path) ?: emptyArray()

        override fun open(path: String): InputStream = assets.open(path)
    }
}
