package io.github.lucaaszsx.betteraethersx2

import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import io.github.lucaaszsx.betteraethersx2.bios.BiosImportResult
import io.github.lucaaszsx.betteraethersx2.bios.BiosImporter
import io.github.lucaaszsx.betteraethersx2.runtime.EmulatorRuntime
import io.github.lucaaszsx.betteraethersx2.runtime.NativeRuntime
import io.github.lucaaszsx.betteraethersx2.ui.BetterTheme
import io.github.lucaaszsx.betteraethersx2.ui.LibraryScreen
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.FileNotFoundException
import java.io.IOException

class MainActivity : ComponentActivity() {
    private val runtime: EmulatorRuntime by lazy { NativeRuntime() }
    private val biosImporter by lazy { BiosImporter(filesDir.resolve("bios"), runtime) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            var biosImport by remember { mutableStateOf<BiosImportResult?>(null) }
            val scope = rememberCoroutineScope()
            val pickBios =
                rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
                    if (uri == null) return@rememberLauncherForActivityResult
                    scope.launch { biosImport = withContext(Dispatchers.IO) { importBios(uri) } }
                }
            BetterTheme {
                LibraryScreen(
                    coreVersion = runtime.coreVersion,
                    biosImport = biosImport,
                    onImportBios = { pickBios.launch(arrayOf("*/*")) },
                )
            }
        }
    }

    private fun importBios(uri: Uri): BiosImportResult {
        val source =
            try {
                contentResolver.openInputStream(uri) ?: throw FileNotFoundException(uri.toString())
            } catch (error: IOException) {
                return BiosImportResult.ReadFailed(error)
            }
        return biosImporter.import(source, displayName(uri))
    }

    private fun displayName(uri: Uri): String {
        val fallback = uri.lastPathSegment ?: "bios.bin"
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) return cursor.getString(0) ?: fallback
        }
        return fallback
    }
}
