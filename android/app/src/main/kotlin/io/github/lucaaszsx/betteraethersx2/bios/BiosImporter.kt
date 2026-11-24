package io.github.lucaaszsx.betteraethersx2.bios

import io.github.lucaaszsx.betteraethersx2.runtime.BiosIdentity
import io.github.lucaaszsx.betteraethersx2.runtime.EmulatorRuntime
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.security.DigestOutputStream
import java.security.MessageDigest

sealed interface BiosImportResult {
    // knownDumpName is null when the file is a BIOS but not a dump known to be good, which usually means a damaged
    // or modified dump.
    data class Recognized(
        val identity: BiosIdentity,
        val knownDumpName: String?,
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
    private val knownDumps: KnownBiosDumps,
) {
    fun import(
        source: InputStream,
        fileName: String,
    ): BiosImportResult {
        val target = biosDirectory.resolve(File(fileName).name)
        val sha1 = MessageDigest.getInstance("SHA-1")
        val copyError = copy(source, target, sha1)
        if (copyError != null) return BiosImportResult.ReadFailed(copyError)

        val identity = runtime.identifyBios(target)
        if (identity == null) target.delete()
        return if (identity == null) {
            BiosImportResult.NotABios
        } else {
            BiosImportResult.Recognized(identity, knownDumps.nameOf(sha1.digest().toHex()))
        }
    }

    // Copies while hashing, so the file is read once. Returns the error when the copy fails, after removing any
    // partial file.
    private fun copy(
        source: InputStream,
        target: File,
        digest: MessageDigest,
    ): IOException? =
        try {
            biosDirectory.mkdirs()
            source.use { input -> DigestOutputStream(target.outputStream(), digest).use { input.copyTo(it) } }
            null
        } catch (error: IOException) {
            target.delete()
            error
        }
}

private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }
