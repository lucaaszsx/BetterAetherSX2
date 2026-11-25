package io.github.lucaaszsx.betteraethersx2.library

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.core.content.edit

// The folder the player chose for their games. Android keeps the read permission across restarts once the app
// takes it, until the player revokes it in system settings.
class GameFolder(
    private val context: Context,
) {
    private val preferences = context.getSharedPreferences("library", Context.MODE_PRIVATE)

    val uri: Uri?
        get() = preferences.getString(KEY_FOLDER, null)?.let(Uri::parse)

    fun choose(uri: Uri) {
        context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        preferences.edit { putString(KEY_FOLDER, uri.toString()) }
    }

    // False once Android no longer lets the app read the folder.
    fun hasAccess(uri: Uri): Boolean =
        context.contentResolver.persistedUriPermissions.any {
            it.uri == uri &&
                it.isReadPermission
        }

    private companion object {
        const val KEY_FOLDER = "gameFolder"
    }
}
