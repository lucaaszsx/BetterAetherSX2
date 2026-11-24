package io.github.lucaaszsx.betteraethersx2

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import io.github.lucaaszsx.betteraethersx2.runtime.EmulatorRuntime
import io.github.lucaaszsx.betteraethersx2.runtime.NativeRuntime
import io.github.lucaaszsx.betteraethersx2.ui.BetterTheme
import io.github.lucaaszsx.betteraethersx2.ui.LibraryScreen

class MainActivity : ComponentActivity() {
    private val runtime: EmulatorRuntime by lazy { NativeRuntime() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { BetterTheme { LibraryScreen(coreVersion = runtime.coreVersion) } }
    }
}
