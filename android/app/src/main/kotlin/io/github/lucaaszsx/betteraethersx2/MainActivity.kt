package io.github.lucaaszsx.betteraethersx2

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import io.github.lucaaszsx.betteraethersx2.bios.BiosImportResult
import io.github.lucaaszsx.betteraethersx2.bios.BiosImporter
import io.github.lucaaszsx.betteraethersx2.bios.KnownBiosDumps
import io.github.lucaaszsx.betteraethersx2.ui.BetterTheme
import io.github.lucaaszsx.betteraethersx2.ui.LibraryScreen
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.FileNotFoundException
import java.io.IOException

class MainActivity : ComponentActivity() {
    private val app by lazy { application as BetterApp }
    private val biosDirectory by lazy { filesDir.resolve("bios") }
    private val biosImporter by lazy {
        val knownDumps = assets.open("known-bios.txt").use(KnownBiosDumps::parse)
        BiosImporter(biosDirectory, app.runtime, knownDumps)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            var biosImport by remember { mutableStateOf<BiosImportResult?>(null) }
            var hasBios by remember { mutableStateOf(biosDirectory.list().orEmpty().isNotEmpty()) }
            var coreReady by remember { mutableStateOf<Boolean?>(null) }
            var sessionFailure by remember { mutableStateOf<String?>(null) }
            val scope = rememberCoroutineScope()
            LaunchedEffect(Unit) { coreReady = withContext(Dispatchers.IO) { app.prepareRuntime() } }
            val pickBios =
                rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
                    if (uri == null) return@rememberLauncherForActivityResult
                    scope.launch {
                        val result = withContext(Dispatchers.IO) { importBios(uri) }
                        biosImport = result
                        if (result is BiosImportResult.Recognized) hasBios = true
                    }
                }
            val startGame =
                rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
                    sessionFailure = result.data?.getStringExtra(GameActivity.EXTRA_FAILURE_REASON)
                }
            BetterTheme {
                LibraryScreen(
                    coreVersion = app.runtime.coreVersion,
                    coreReady = coreReady,
                    hasBios = hasBios,
                    biosImport = biosImport,
                    sessionFailure = sessionFailure,
                    onImportBios = { pickBios.launch(arrayOf("*/*")) },
                    onStartBios = {
                        sessionFailure = null
                        startGame.launch(Intent(this, GameActivity::class.java))
                    },
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
