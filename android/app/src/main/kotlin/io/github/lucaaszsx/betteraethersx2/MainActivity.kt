package io.github.lucaaszsx.betteraethersx2

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import io.github.lucaaszsx.betteraethersx2.bios.BiosImportResult
import io.github.lucaaszsx.betteraethersx2.bios.BiosImporter
import io.github.lucaaszsx.betteraethersx2.bios.KnownBiosDumps
import io.github.lucaaszsx.betteraethersx2.library.GameCache
import io.github.lucaaszsx.betteraethersx2.library.GameFolder
import io.github.lucaaszsx.betteraethersx2.library.GameScanner
import io.github.lucaaszsx.betteraethersx2.library.LibraryState
import io.github.lucaaszsx.betteraethersx2.library.TreeDocumentSource
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
    private val gameFolder by lazy { GameFolder(this) }
    private val gameCache by lazy { GameCache(filesDir.resolve("games.tsv")) }

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
            val (library, chooseFolder) = rememberLibrary(coreReady)
            BetterTheme {
                LibraryScreen(
                    coreVersion = app.runtime.coreVersion,
                    coreReady = coreReady,
                    hasBios = hasBios,
                    library = library,
                    biosImport = biosImport,
                    sessionFailure = sessionFailure,
                    onChooseFolder = chooseFolder,
                    onImportBios = { pickBios.launch(arrayOf("*/*")) },
                    onStartBios = {
                        sessionFailure = null
                        startGame.launch(Intent(this, GameActivity::class.java))
                    },
                )
            }
        }
    }

    // The game list for the chosen folder, scanned once the core is ready, and the action that picks a folder.
    @Composable
    private fun rememberLibrary(coreReady: Boolean?): Pair<LibraryState, () -> Unit> {
        var folder by remember { mutableStateOf(gameFolder.uri) }
        var library by remember { mutableStateOf<LibraryState>(LibraryState.NoFolder) }
        LaunchedEffect(folder, coreReady) {
            val uri = folder
            if (coreReady != true || uri == null) return@LaunchedEffect
            library = LibraryState.Scanning(0)
            library =
                withContext(Dispatchers.IO) { scanLibrary(uri) { found -> library = LibraryState.Scanning(found) } }
        }
        val pickFolder =
            rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
                if (uri == null) return@rememberLauncherForActivityResult
                gameFolder.choose(uri)
                folder = uri
            }
        return library to { pickFolder.launch(gameFolder.uri) }
    }

    private fun scanLibrary(
        folder: Uri,
        onFound: (Int) -> Unit,
    ): LibraryState {
        if (!gameFolder.hasAccess(folder)) return LibraryState.AccessLost
        val scanner = GameScanner(TreeDocumentSource(contentResolver, folder), app.runtime::identifyGame)
        return try {
            val (games, entries) = scanner.scan(gameCache.load(), onFound)
            gameCache.save(entries)
            LibraryState.Loaded(games)
        } catch (error: SecurityException) {
            Log.w(TAG, "Lost access to $folder", error)
            LibraryState.AccessLost
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

    private companion object {
        const val TAG = "BetterAetherSX2"
    }
}
