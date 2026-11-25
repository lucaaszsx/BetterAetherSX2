package io.github.lucaaszsx.betteraethersx2.library

import io.github.lucaaszsx.betteraethersx2.runtime.GameIdentity
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class GameCacheTest {
    @get:Rule
    val folder = TemporaryFolder()

    @Test
    fun readsBackWhatItSaved() {
        val cache = GameCache(folder.root.resolve("games.tsv"))
        val entries =
            mapOf(
                "content://a/Game.iso" to
                    CachedGame(4_700_000_000, 1_700_000_000, GameIdentity("SLUS-20062", "Game", "NTSC-U")),
                "content://a/notes.iso" to CachedGame(12, 34, null),
            )
        cache.save(entries)
        assertEquals(entries, cache.load())
    }

    @Test
    fun keepsTabsInTitlesFromBreakingTheLine() {
        val cache = GameCache(folder.root.resolve("games.tsv"))
        cache.save(mapOf("content://a/b.chd" to CachedGame(1, 2, GameIdentity("SLES-1", "A\tB", "PAL"))))
        assertEquals(
            "A B",
            cache
                .load()
                .getValue("content://a/b.chd")
                .identity
                ?.title,
        )
    }

    @Test
    fun startsEmptyWithoutAFile() {
        assertEquals(emptyMap<String, CachedGame>(), GameCache(folder.root.resolve("missing.tsv")).load())
    }
}
