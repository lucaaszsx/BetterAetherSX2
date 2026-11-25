package io.github.lucaaszsx.betteraethersx2.runtime

import java.io.File
import java.io.InputStream

// Where the core's bundled files (shaders, fonts, game database) come from. The app reads them from its APK
// assets; tests use a map.
interface ResourceSource {
    // The names inside a directory, or an empty array for a file.
    fun list(path: String): Array<String>

    fun open(path: String): InputStream
}

// The core reads its resources from a real directory, so the copy in the APK is extracted once per install.
class CoreResources(
    private val source: ResourceSource,
    private val target: File,
) {
    // installStamp changes with every install or update of the app, so a stale copy is replaced.
    fun install(installStamp: Long) {
        val stampFile = target.resolve(STAMP_FILE)
        if (stampFile.isFile && stampFile.readText() == installStamp.toString()) return
        target.deleteRecursively()
        copy(ROOT, target)
        stampFile.writeText(installStamp.toString())
    }

    private fun copy(
        path: String,
        destination: File,
    ) {
        val children = source.list(path)
        if (children.isEmpty()) {
            destination.parentFile?.mkdirs()
            source.open(path).use { input -> destination.outputStream().use { input.copyTo(it) } }
            return
        }
        destination.mkdirs()
        for (child in children) copy("$path/$child", destination.resolve(child))
    }

    private companion object {
        const val ROOT = "resources"
        const val STAMP_FILE = ".installed"
    }
}
