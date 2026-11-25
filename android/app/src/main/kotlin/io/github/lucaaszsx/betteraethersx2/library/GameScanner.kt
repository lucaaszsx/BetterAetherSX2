package io.github.lucaaszsx.betteraethersx2.library

import io.github.lucaaszsx.betteraethersx2.runtime.GameIdentity

// Walks the game folder and its subfolders, and identifies each disc image with the core. Files the cache already
// knows, with the same size and modification time, are not opened again.
class GameScanner(
    private val source: DocumentSource,
    private val identify: (uri: String) -> GameIdentity?,
) {
    // Returns the games sorted by title, and the cache entries to save. onFound hears the running count of games.
    fun scan(
        cache: Map<String, CachedGame>,
        onFound: (count: Int) -> Unit,
    ): Pair<List<Game>, Map<String, CachedGame>> {
        val games = mutableListOf<Game>()
        val seen = mutableMapOf<String, CachedGame>()
        val pending = ArrayDeque(listOf(source.rootId))
        while (pending.isNotEmpty()) {
            for (document in source.children(pending.removeFirst())) {
                if (document.isDirectory) {
                    pending.addLast(document.id)
                } else if (GameFiles.isGame(document.name)) {
                    val entry = cachedOrIdentified(document, cache[document.uri])
                    seen[document.uri] = entry
                    val identity = entry.identity ?: continue
                    games += Game(document.uri, document.name, identity)
                    onFound(games.size)
                }
            }
        }
        return games.sortedBy { it.identity.title.lowercase() } to seen
    }

    private fun cachedOrIdentified(
        document: Document,
        cached: CachedGame?,
    ): CachedGame {
        if (cached != null && cached.size == document.size && cached.modified == document.modified) return cached
        return CachedGame(document.size, document.modified, identify(document.uri))
    }
}
