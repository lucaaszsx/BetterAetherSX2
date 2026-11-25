package io.github.lucaaszsx.betteraethersx2.library

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GameFilesTest {
    @Test
    fun acceptsSingleFileDiscImagesInAnyCase() {
        assertTrue(GameFiles.isGame("Game.iso"))
        assertTrue(GameFiles.isGame("Game (USA).CHD"))
        assertTrue(GameFiles.isGame("game.zso"))
    }

    @Test
    fun rejectsOtherFiles() {
        assertFalse(GameFiles.isGame("Game.cue"))
        assertFalse(GameFiles.isGame("notes.txt"))
        assertFalse(GameFiles.isGame("iso"))
    }
}
