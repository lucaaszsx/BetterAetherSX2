package io.github.lucaaszsx.betteraethersx2.library

import io.github.lucaaszsx.betteraethersx2.runtime.GameIdentity
import java.io.File

// What the scanner learned about a file. A null identity means the core did not recognize it as a PS2 game, so
// the file is skipped until it changes.
data class CachedGame(
    val size: Long,
    val modified: Long,
    val identity: GameIdentity?,
)

// Remembers scanned files between launches, so unchanged disc images are not opened again. One line per file:
// uri, size, modified, serial, title, region, separated by tabs.
class GameCache(
    private val file: File,
) {
    fun load(): Map<String, CachedGame> {
        if (!file.exists()) return emptyMap()
        return file.readLines().mapNotNull(::parse).toMap()
    }

    fun save(entries: Map<String, CachedGame>) {
        file.writeText(entries.entries.joinToString("") { (uri, game) -> format(uri, game) })
    }

    // Null for a line this version cannot read, which is then scanned again.
    private fun parse(line: String): Pair<String, CachedGame>? {
        val fields = line.split(SEPARATOR)
        val bytes = fields.getOrNull(1)?.toLongOrNull()
        if (fields.size != FIELD_COUNT || bytes == null) return null
        val (uri, _, modified) = fields
        val (serial, title, region) = fields.subList(IDENTITY_START, FIELD_COUNT)
        val identity = if (serial.isEmpty()) null else GameIdentity(serial, title, region)
        return uri to CachedGame(bytes, modified.toLongOrNull() ?: 0, identity)
    }

    private fun format(
        uri: String,
        game: CachedGame,
    ): String {
        val identity = game.identity
        val fields =
            listOf(
                uri,
                game.size.toString(),
                game.modified.toString(),
                identity?.serial.orEmpty(),
                identity?.title.orEmpty(),
                identity?.region.orEmpty(),
            )
        return fields.joinToString(SEPARATOR) { it.replace(Regex("[\t\n]"), " ") } + "\n"
    }

    private companion object {
        const val SEPARATOR = "\t"
        const val FIELD_COUNT = 6

        // The serial, title and region follow the uri, size and modification time.
        const val IDENTITY_START = 3
    }
}
