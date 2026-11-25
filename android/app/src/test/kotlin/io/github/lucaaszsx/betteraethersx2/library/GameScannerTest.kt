package io.github.lucaaszsx.betteraethersx2.library

import io.github.lucaaszsx.betteraethersx2.runtime.GameIdentity
import org.junit.Assert.assertEquals
import org.junit.Test

class GameScannerTest {
    private val tree =
        object : DocumentSource {
            override val rootId = "root"

            override fun children(directoryId: String): List<Document> =
                when (directoryId) {
                    "root" -> listOf(file("b.iso", size = 10), directory("sub"), file("readme.txt", size = 1))
                    "sub" -> listOf(file("a.chd", size = 20), file("broken.iso", size = 30))
                    else -> emptyList()
                }
        }

    private val opened = mutableListOf<String>()
    private val scanner =
        GameScanner(tree) { uri ->
            opened += uri
            when (uri) {
                "uri:b.iso" -> GameIdentity("SLUS-2", "Beta", "NTSC-U")
                "uri:a.chd" -> GameIdentity("SLES-1", "alpha", "PAL")
                else -> null
            }
        }

    @Test
    fun findsGamesInSubfoldersSortedByTitle() {
        val counts = mutableListOf<Int>()
        val (games, _) = scanner.scan(emptyMap()) { counts += it }
        assertEquals(listOf("alpha", "Beta"), games.map { it.identity.title })
        assertEquals(listOf(1, 2), counts)
    }

    @Test
    fun skipsFilesTheCoreDoesNotRecognize() {
        val (_, cache) = scanner.scan(emptyMap()) {}
        assertEquals(null, cache.getValue("uri:broken.iso").identity)
        assertEquals(setOf("uri:b.iso", "uri:a.chd", "uri:broken.iso"), cache.keys)
    }

    @Test
    fun reusesUnchangedFilesFromTheCache() {
        val (_, cache) = scanner.scan(emptyMap()) {}
        opened.clear()
        scanner.scan(cache) {}
        assertEquals(emptyList<String>(), opened)
    }

    @Test
    fun identifiesAFileAgainWhenItChanged() {
        val (_, cache) = scanner.scan(emptyMap()) {}
        opened.clear()
        val stale = cache + ("uri:b.iso" to cache.getValue("uri:b.iso").copy(size = 999))
        scanner.scan(stale) {}
        assertEquals(listOf("uri:b.iso"), opened)
    }

    private fun file(
        name: String,
        size: Long,
    ) = Document(id = name, uri = "uri:$name", name = name, isDirectory = false, size = size, modified = 0)

    private fun directory(name: String) =
        Document(id = name, uri = "uri:$name", name = name, isDirectory = true, size = 0, modified = 0)
}
