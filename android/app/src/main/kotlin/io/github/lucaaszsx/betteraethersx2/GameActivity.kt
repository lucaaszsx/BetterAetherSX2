package io.github.lucaaszsx.betteraethersx2

import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

// Full-screen game view. The session lives as long as the surface: it starts when the surface appears and stops
// when it goes away, so leaving the app ends the session for now.
class GameActivity :
    ComponentActivity(),
    SurfaceHolder.Callback {
    private val runtime by lazy { (application as BetterApp).runtime }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        hideSystemBars()
        setContentView(SurfaceView(this).also { it.holder.addCallback(this) })
    }

    override fun surfaceCreated(holder: SurfaceHolder) = runtime.startSession(holder.surface)

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) = runtime.resizeSurface(width, height)

    override fun surfaceDestroyed(holder: SurfaceHolder) = runtime.stopSession()

    private fun hideSystemBars() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }
}
