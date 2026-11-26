package io.github.lucaaszsx.betteraethersx2

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.Window
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import io.github.lucaaszsx.betteraethersx2.input.GamepadInput
import io.github.lucaaszsx.betteraethersx2.input.PadState
import io.github.lucaaszsx.betteraethersx2.input.TouchInput
import kotlin.concurrent.thread

// Full-screen game view. The session lives as long as the activity: it starts with the first surface, pauses while
// the activity is in the background, moves to each new surface, and stops when the activity is destroyed. The
// manifest keeps rotation and folding from recreating the activity. When the session fails, the activity closes and
// hands the core's reason back to the screen that opened it.
class GameActivity :
    ComponentActivity(),
    SurfaceHolder.Callback {
    private val runtime by lazy { (application as BetterApp).runtime }
    private var sessionStarted = false
    private val gamepad by lazy { GamepadInput(PadState(runtime::setPadInput)) }
    private val touch by lazy { TouchInput(runtime::touch) }
    private lateinit var gameView: SurfaceView
    private var coreReady = false
    private var resumed = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.hideSystemBars()
        gameView = SurfaceView(this)
        gameView.holder.addCallback(this)
        // The game draws its own touch controls, so the view only hands the fingers over.
        gameView.setOnTouchListener { _, event -> true.also { touch.onTouch(event) } }
        // This can be the first screen of the process: started from adb, or restored after Android ended the process
        // in the background. The surface starts the session, so it goes on screen only once the core is ready.
        thread(name = "PrepareCore") {
            val ready = (application as BetterApp).prepareRuntime()
            runOnUiThread { onCorePrepared(ready) }
        }
    }

    private fun onCorePrepared(ready: Boolean) {
        if (isDestroyed) return
        // The library screen says why the core is not ready, so a failure here only closes the game.
        if (!ready) return finish()
        coreReady = true
        showGameWhenResumed()
    }

    // A session started while paused would boot running: a pause sent before its VM exists is dropped.
    private fun showGameWhenResumed() {
        if (coreReady && resumed && gameView.parent == null) setContentView(gameView)
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        runtime.setPaused(false)
        showGameWhenResumed()
    }

    override fun onPause() {
        resumed = false
        runtime.setPaused(true)
        super.onPause()
    }

    override fun onDestroy() {
        runtime.stopSession()
        super.onDestroy()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean = gamepad.onKey(event) || super.dispatchKeyEvent(event)

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean =
        gamepad.onMotion(event) || super.dispatchGenericMotionEvent(event)

    override fun surfaceCreated(holder: SurfaceHolder) {
        if (sessionStarted) return runtime.attachSurface(holder.surface)
        sessionStarted = true
        runtime.setBenchmark(intent.getBooleanExtra(EXTRA_BENCHMARK, false))
        runtime.startSession(this, holder.surface, intent.getStringExtra(EXTRA_GAME)) { failed, reason ->
            if (failed) runOnUiThread { finishWithFailure(reason) }
        }
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) = runtime.resizeSurface(width, height, resources.displayMetrics.density)

    override fun surfaceDestroyed(holder: SurfaceHolder) = runtime.detachSurface()

    private fun finishWithFailure(reason: String) {
        setResult(RESULT_OK, Intent().putExtra(EXTRA_FAILURE_REASON, reason))
        finish()
    }

    companion object {
        // The disc image to boot, as a URI string. Without it the session runs the BIOS alone.
        const val EXTRA_GAME = "io.github.lucaaszsx.betteraethersx2.GAME"

        // Turns on benchmark logging for this session. Only debug builds export this activity to adb.
        const val EXTRA_BENCHMARK = "io.github.lucaaszsx.betteraethersx2.BENCHMARK"

        // Set on the result when the session failed: the core's reason, possibly empty.
        const val EXTRA_FAILURE_REASON = "io.github.lucaaszsx.betteraethersx2.FAILURE_REASON"
    }
}

private fun Window.hideSystemBars() {
    WindowCompat.setDecorFitsSystemWindows(this, false)
    WindowInsetsControllerCompat(this, decorView).apply {
        hide(WindowInsetsCompat.Type.systemBars())
        systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}
