package io.github.lucaaszsx.betteraethersx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.lucaaszsx.betteraethersx2.R
import io.github.lucaaszsx.betteraethersx2.bios.BiosImportResult
import io.github.lucaaszsx.betteraethersx2.library.Game
import io.github.lucaaszsx.betteraethersx2.library.LibraryState

private val ScreenPadding = 24.dp
private val LineSpacing = 12.dp
private val GameRowPadding = 8.dp
private val PARAGRAPH_BREAK = Regex("""\n\s*\n""")

// A plain, working library until the app's visual direction is chosen.
@Composable
fun LibraryScreen(
    coreVersion: String,
    // Null while the core is still being prepared.
    coreReady: Boolean?,
    hasBios: Boolean,
    library: LibraryState,
    biosImport: BiosImportResult?,
    // The core's reason the last session failed, or null when it did not fail.
    sessionFailure: String?,
    onChooseFolder: () -> Unit,
    onImportBios: () -> Unit,
    onStartBios: () -> Unit,
) {
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background)
                .safeDrawingPadding()
                .padding(ScreenPadding),
        verticalArrangement = Arrangement.spacedBy(LineSpacing),
    ) {
        FlowRow(horizontalArrangement = Arrangement.spacedBy(LineSpacing)) {
            val folderAction =
                if (library ==
                    LibraryState.NoFolder
                ) {
                    R.string.choose_game_folder
                } else {
                    R.string.change_game_folder
                }
            Button(onClick = onChooseFolder, enabled = coreReady == true) { Text(stringResource(folderAction)) }
            Button(onClick = onImportBios) { Text(stringResource(R.string.import_bios)) }
            if (hasBios) {
                Button(onClick = onStartBios, enabled = coreReady == true) { Text(stringResource(R.string.start_bios)) }
            }
        }
        if (coreReady == false) Message(stringResource(R.string.core_not_ready), MaterialTheme.colorScheme.error)
        if (sessionFailure != null) Message(sessionFailureMessage(sessionFailure), MaterialTheme.colorScheme.error)
        if (biosImport != null) Message(biosImportMessage(biosImport), MaterialTheme.colorScheme.onBackground)
        Box(modifier = Modifier.weight(1f)) { GameLibrary(library) }
        Text(
            text = stringResource(R.string.core_version, coreVersion),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun GameLibrary(library: LibraryState) {
    when (library) {
        LibraryState.NoFolder -> {
            Message(stringResource(R.string.library_empty), MaterialTheme.colorScheme.onBackground)
        }

        is LibraryState.Scanning -> {
            Message(
                pluralStringResource(R.plurals.scanning_folder, library.found, library.found),
                MaterialTheme.colorScheme.onBackground,
            )
        }

        LibraryState.AccessLost -> {
            Message(stringResource(R.string.folder_access_lost), MaterialTheme.colorScheme.error)
        }

        is LibraryState.Loaded -> {
            GameList(library.games)
        }
    }
}

@Composable
private fun GameList(games: List<Game>) {
    if (games.isEmpty()) return Message(stringResource(R.string.folder_empty), MaterialTheme.colorScheme.onBackground)
    LazyColumn(modifier = Modifier.fillMaxSize()) {
        items(games, key = { it.uri }) { game -> GameRow(game) }
    }
}

@Composable
private fun GameRow(game: Game) {
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = GameRowPadding)) {
        Text(
            text = game.identity.title,
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onBackground,
        )
        Text(
            text = stringResource(R.string.game_details, game.identity.serial, game.identity.region),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun Message(
    text: String,
    color: Color,
) {
    Text(text = text, style = MaterialTheme.typography.bodyMedium, color = color)
}

@Composable
private fun biosImportMessage(result: BiosImportResult): String =
    when (result) {
        is BiosImportResult.Recognized -> recognizedBiosMessage(result)
        BiosImportResult.NotABios -> stringResource(R.string.bios_not_recognized)
        is BiosImportResult.ReadFailed -> stringResource(R.string.bios_read_failed)
    }

@Composable
private fun sessionFailureMessage(reason: String): String {
    val sentence = asSentence(reason) ?: return stringResource(R.string.session_failed_unknown)
    return stringResource(R.string.session_failed, sentence)
}

// The core's messages are often fragments without a final period, and some go on with desktop instructions in
// later paragraphs that do not apply here. The app keeps the first paragraph, as a whole sentence. Null when there
// is no reason to show.
internal fun asSentence(reason: String): String? {
    val trimmed = reason.split(PARAGRAPH_BREAK).firstOrNull { it.isNotBlank() }?.trim() ?: return null
    return if (trimmed.last() in ".!?") trimmed else "$trimmed."
}

@Composable
private fun recognizedBiosMessage(result: BiosImportResult.Recognized): String {
    val recognized = stringResource(R.string.bios_recognized, result.identity.region, result.identity.version)
    if (result.knownDumpName != null) return recognized
    return "$recognized ${stringResource(R.string.bios_unknown_dump)}"
}
