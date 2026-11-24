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

@Composable
fun LibraryScreen(
    coreVersion: String,
    biosImport: BiosImportResult?,
    onImportBios: () -> Unit,
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
        is BiosImportResult.Recognized -> {
            val recognized = stringResource(R.string.bios_recognized, result.identity.region, result.identity.version)
            if (result.knownDumpName != null) recognized else "$recognized ${stringResource(R.string.bios_unknown_dump)}"
        }
        BiosImportResult.NotABios -> stringResource(R.string.bios_not_recognized)
        is BiosImportResult.ReadFailed -> stringResource(R.string.bios_read_failed)
    }
