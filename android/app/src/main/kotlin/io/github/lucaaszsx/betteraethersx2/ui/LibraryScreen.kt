package io.github.lucaaszsx.betteraethersx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import io.github.lucaaszsx.betteraethersx2.R
import io.github.lucaaszsx.betteraethersx2.bios.BiosImportResult

private val ScreenPadding = 24.dp
private val LineSpacing = 12.dp
private val PARAGRAPH_BREAK = Regex("""\n\s*\n""")

@Composable
fun LibraryScreen(
    coreVersion: String,
    // Null while the core is still being prepared.
    coreReady: Boolean?,
    hasBios: Boolean,
    biosImport: BiosImportResult?,
    // The core's reason the last session failed, or null when it did not fail.
    sessionFailure: String?,
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
        verticalArrangement = Arrangement.spacedBy(LineSpacing, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            text = stringResource(R.string.library_empty),
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onBackground,
            textAlign = TextAlign.Center,
        )
        Button(onClick = onImportBios) { Text(stringResource(R.string.import_bios)) }
        if (hasBios) {
            Button(onClick = onStartBios, enabled = coreReady == true) { Text(stringResource(R.string.start_bios)) }
        }
        if (coreReady == false) {
            Text(
                text = stringResource(R.string.core_not_ready),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.error,
                textAlign = TextAlign.Center,
            )
        }
        if (sessionFailure != null) {
            Text(
                text = sessionFailureMessage(sessionFailure),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.error,
                textAlign = TextAlign.Center,
            )
        }
        if (biosImport != null) {
            Text(
                text = biosImportMessage(biosImport),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onBackground,
                textAlign = TextAlign.Center,
            )
        }
        Text(
            text = stringResource(R.string.core_version, coreVersion),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
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
