package io.github.lucaaszsx.betteraethersx2.bios

import java.io.InputStream

// Good BIOS dumps by SHA-1, read from assets/known-bios.txt (one "<sha1> <dump name>" per line).
class KnownBiosDumps(
    private val namesBySha1: Map<String, String>,
) {
    // The dump name for a known hash, or null when the dump is not in the list.
    fun nameOf(sha1: String): String? = namesBySha1[sha1.lowercase()]

    companion object {
        fun parse(source: InputStream): KnownBiosDumps {
            val entries =
                source.bufferedReader().useLines { lines ->
                    lines
                        .map { it.trim() }
                        .filter { it.isNotEmpty() && !it.startsWith("#") }
                        .map { it.substringBefore(' ').lowercase() to it.substringAfter(' ') }
                        .toMap()
                }
            return KnownBiosDumps(entries)
        }
    }
}
