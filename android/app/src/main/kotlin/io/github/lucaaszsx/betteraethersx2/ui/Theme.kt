package io.github.lucaaszsx.betteraethersx2.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Base palette. The full visual identity replaces this once the visual direction is chosen.
private val Ink0 = Color(0xFF0A0D12)
private val Ink1 = Color(0xFF11161D)
private val Paper0 = Color(0xFFEDF1F5)
private val Paper1 = Color(0xFFAEB7C2)
private val Azure = Color(0xFF4F9CF9)
private val ErrorRed = Color(0xFFE5604D)

private val DarkColors =
    darkColorScheme(
        primary = Azure,
        onPrimary = Ink0,
        background = Ink0,
        onBackground = Paper0,
        surface = Ink1,
        onSurface = Paper0,
        onSurfaceVariant = Paper1,
        error = ErrorRed,
    )

@Composable
fun BetterTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = DarkColors, content = content)
}
