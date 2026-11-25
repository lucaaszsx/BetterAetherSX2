package io.github.lucaaszsx.betteraethersx2.library

// One entry of a folder the player granted access to.
data class Document(
    val id: String,
    val uri: String,
    val name: String,
    val isDirectory: Boolean,
    val size: Long,
    val modified: Long,
)

// Lists folder contents, so the scanner can be tested without Android's document providers.
interface DocumentSource {
    val rootId: String

    // Throws SecurityException when access to the folder was taken away.
    fun children(directoryId: String): List<Document>
}
