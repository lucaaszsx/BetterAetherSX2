package io.github.lucaaszsx.betteraethersx2.runtime

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import java.io.FileNotFoundException

// Opens content:// URIs for the native core, which reads games from folders chosen through the Storage Access
// Framework. Called from native threads through JNI, so both functions log failures instead of throwing.
object ContentAccess {
    private const val TAG = "BetterAetherSX2"
    private const val MILLIS_PER_SECOND = 1000L
    private lateinit var resolver: ContentResolver

    fun install(resolver: ContentResolver) {
        this.resolver = resolver
    }

    // A new read-only file descriptor the caller owns, or -1.
    @JvmStatic
    fun openFd(uri: String): Int =
        try {
            resolver.openFileDescriptor(Uri.parse(uri), "r")?.detachFd() ?: -1
        } catch (error: FileNotFoundException) {
            Log.w(TAG, "Couldn't open $uri", error)
            -1
        } catch (error: SecurityException) {
            Log.w(TAG, "No access to $uri", error)
            -1
        }

    // [size in bytes, modification time in seconds], or null when the document cannot be read.
    @JvmStatic
    fun stat(uri: String): LongArray? {
        val columns = arrayOf(DocumentsContract.Document.COLUMN_SIZE, DocumentsContract.Document.COLUMN_LAST_MODIFIED)
        return try {
            resolver.query(Uri.parse(uri), columns, null, null, null)?.use { cursor ->
                if (!cursor.moveToFirst()) return null
                longArrayOf(cursor.getLong(0), cursor.getLong(1) / MILLIS_PER_SECOND)
            }
        } catch (error: SecurityException) {
            Log.w(TAG, "No access to $uri", error)
            null
        }
    }
}
