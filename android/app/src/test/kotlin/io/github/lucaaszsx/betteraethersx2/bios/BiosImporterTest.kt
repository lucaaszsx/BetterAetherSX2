package io.github.lucaaszsx.betteraethersx2.bios

import io.github.lucaaszsx.betteraethersx2.runtime.BiosIdentity
import io.github.lucaaszsx.betteraethersx2.runtime.FakeRuntime
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.IOException
import java.io.InputStream

class BiosImporterTest {
    @get:Rule
    val folder = TemporaryFolder()

    private val contents = ByteArray(16) { it.toByte() }

    // SHA-1 of `contents`, so the test file counts as a known dump.
    private val contentsSha1 = "56178b86a57fac22899a9964185c2cc96e7da589"
    private val knownDumps = KnownBiosDumps(mapOf(contentsSha1 to "Test dump"))

    @Test
    fun keepsARecognizedBiosInTheBiosFolder() {
        val identity = BiosIdentity(region = "Europe", version = "2.00")
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(bios = identity), knownDumps)

        val result = importer.import(contents.inputStream(), "scph-70004.bin")

        assertEquals(BiosImportResult.Recognized(identity, knownDumpName = "Test dump"), result)
        assertTrue(biosDirectory.resolve("scph-70004.bin").readBytes().contentEquals(contents))
    }

    @Test
    fun acceptsABiosThatIsNotAKnownDumpButSaysSo() {
        val identity = BiosIdentity(region = "Japan", version = "1.00")
        val importer = BiosImporter(folder.root, FakeRuntime(bios = identity), KnownBiosDumps(emptyMap()))

        val result = importer.import(contents.inputStream(), "modified.bin")

        assertEquals(BiosImportResult.Recognized(identity, knownDumpName = null), result)
    }

    @Test
    fun deletesAFileTheCoreDoesNotRecognize() {
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(bios = null), knownDumps)

        val result = importer.import(contents.inputStream(), "notes.txt")

        assertEquals(BiosImportResult.NotABios, result)
        assertFalse(biosDirectory.resolve("notes.txt").exists())
    }

    @Test
    fun reportsAReadErrorAndLeavesNoPartialFile() {
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(), knownDumps)
        val failing =
            object : InputStream() {
                override fun read(): Int = throw IOException("The file was removed while copying.")
            }

        val result = importer.import(failing, "bios.bin")

        assertTrue(result is BiosImportResult.ReadFailed)
        assertFalse(biosDirectory.resolve("bios.bin").exists())
    }

    @Test
    fun ignoresDirectoriesInThePickedFileName() {
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(), knownDumps)

        importer.import(contents.inputStream(), "../../escape.bin")

        assertTrue(biosDirectory.resolve("escape.bin").exists())
        assertFalse(folder.root.resolve("escape.bin").exists())
    }
}
