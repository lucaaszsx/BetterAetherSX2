package io.github.lucaaszsx.betteraethersx2.bios

import io.github.lucaaszsx.betteraethersx2.runtime.BiosIdentity
import io.github.lucaaszsx.betteraethersx2.runtime.EmulatorRuntime
import java.io.File
import java.io.IOException
import java.io.InputStream

sealed interface BiosImportResult {
    data class Recognized(
        val identity: BiosIdentity,
    ) : BiosImportResult

    data object NotABios : BiosImportResult

    data class ReadFailed(
        val cause: IOException,
    ) : BiosImportResult
}

// Copies a BIOS the user picked into the app's BIOS folder and keeps it only if the core recognizes it.
// The copy is needed because the core reads the BIOS from its own folder at every boot.
class BiosImporter(
    private val biosDirectory: File,
    private val runtime: EmulatorRuntime,
) {
    fun import(
        source: InputStream,
        fileName: String,
    ): BiosImportResult {
        val target = biosDirectory.resolve(File(fileName).name)
        val copyError = copy(source, target)
        if (copyError != null) return BiosImportResult.ReadFailed(copyError)

        val identity = runtime.identifyBios(target)
        if (identity == null) target.delete()
        return if (identity == null) BiosImportResult.NotABios else BiosImportResult.Recognized(identity)
    }

    // Returns the error when the copy fails, after removing any partial file.
    private fun copy(
        source: InputStream,
        target: File,
    ): IOException? =
        try {
            biosDirectory.mkdirs()
            source.use { input -> target.outputStream().use { input.copyTo(it) } }
            null
        } catch (error: IOException) {
            target.delete()
            error
        }
}
