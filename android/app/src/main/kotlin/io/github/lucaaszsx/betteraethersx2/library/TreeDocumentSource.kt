package io.github.lucaaszsx.betteraethersx2.library

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document.COLUMN_DISPLAY_NAME
import android.provider.DocumentsContract.Document.COLUMN_DOCUMENT_ID
import android.provider.DocumentsContract.Document.COLUMN_LAST_MODIFIED
import android.provider.DocumentsContract.Document.COLUMN_MIME_TYPE
import android.provider.DocumentsContract.Document.COLUMN_SIZE
import android.provider.DocumentsContract.Document.MIME_TYPE_DIR

// Lists a folder tree the player granted through the system folder picker.
class TreeDocumentSource(
    private val resolver: ContentResolver,
    private val treeUri: Uri,
) : DocumentSource {
    override val rootId: String = DocumentsContract.getTreeDocumentId(treeUri)

    override fun children(directoryId: String): List<Document> {
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, directoryId)
        val columns =
            arrayOf(COLUMN_DOCUMENT_ID, COLUMN_DISPLAY_NAME, COLUMN_MIME_TYPE, COLUMN_SIZE, COLUMN_LAST_MODIFIED)
        val documents = mutableListOf<Document>()
        resolver.query(childrenUri, columns, null, null, null)?.use { cursor ->
            while (cursor.moveToNext()) {
                val id = cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_DOCUMENT_ID))
                documents +=
                    Document(
                        id = id,
                        uri = DocumentsContract.buildDocumentUriUsingTree(treeUri, id).toString(),
                        name = cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_DISPLAY_NAME)),
                        isDirectory = cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_MIME_TYPE)) == MIME_TYPE_DIR,
                        size = cursor.getLong(cursor.getColumnIndexOrThrow(COLUMN_SIZE)),
                        modified = cursor.getLong(cursor.getColumnIndexOrThrow(COLUMN_LAST_MODIFIED)),
                    )
            }
        }
        return documents
    }
}
