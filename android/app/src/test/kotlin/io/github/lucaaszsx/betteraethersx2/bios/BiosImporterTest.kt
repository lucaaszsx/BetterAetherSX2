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

    @Test
    fun keepsARecognizedBiosInTheBiosFolder() {
        val identity = BiosIdentity(region = "Europe", version = "2.00")
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(bios = identity))

        val result = importer.import(contents.inputStream(), "scph-70004.bin")

        assertEquals(BiosImportResult.Recognized(identity), result)
        assertTrue(biosDirectory.resolve("scph-70004.bin").readBytes().contentEquals(contents))
    }

    @Test
    fun deletesAFileTheCoreDoesNotRecognize() {
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime(bios = null))

        val result = importer.import(contents.inputStream(), "notes.txt")

        assertEquals(BiosImportResult.NotABios, result)
        assertFalse(biosDirectory.resolve("notes.txt").exists())
    }

    @Test
    fun reportsAReadErrorAndLeavesNoPartialFile() {
        val biosDirectory = folder.root.resolve("bios")
        val importer = BiosImporter(biosDirectory, FakeRuntime())
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
        val importer = BiosImporter(biosDirectory, FakeRuntime())

        importer.import(contents.inputStream(), "../../escape.bin")

        assertTrue(biosDirectory.resolve("escape.bin").exists())
        assertFalse(folder.root.resolve("escape.bin").exists())
    }
}
