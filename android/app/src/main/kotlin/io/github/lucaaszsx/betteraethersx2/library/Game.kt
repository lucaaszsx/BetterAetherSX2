package io.github.lucaaszsx.betteraethersx2.library

import io.github.lucaaszsx.betteraethersx2.runtime.GameIdentity

// A disc image in the game folder that the core recognized as a PS2 game.
data class Game(
    val uri: String,
    val fileName: String,
    val identity: GameIdentity,
)

// Disc image formats the core reads from a single file. BIN/CUE is left out: the CUE sheet names sibling files,
// which a document URI cannot reach.
object GameFiles {
    private val EXTENSIONS = setOf("iso", "chd", "cso", "zso")

    fun isGame(fileName: String): Boolean = fileName.substringAfterLast('.', "").lowercase() in EXTENSIONS
}
