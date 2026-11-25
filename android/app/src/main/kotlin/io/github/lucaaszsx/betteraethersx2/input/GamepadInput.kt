package io.github.lucaaszsx.betteraethersx2.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

// Turns a physical controller's events into pad input. Each method returns false for events that are not from a
// gamepad, so every other key, Back included, keeps its usual meaning.
class GamepadInput(
    private val pad: PadState,
) {
    fun onKey(event: KeyEvent): Boolean {
        val bind = GamepadMapping.bindForKey(event.keyCode)
        val fromGamepad = event.isFromSource(InputDevice.SOURCE_GAMEPAD) || event.isFromSource(InputDevice.SOURCE_DPAD)
        if (bind == null || !fromGamepad) return false
        if (event.action == KeyEvent.ACTION_DOWN) pad.set(bind, 1f)
        if (event.action == KeyEvent.ACTION_UP) pad.set(bind, 0f)
        return true
    }

    fun onMotion(event: MotionEvent): Boolean {
        if (!event.isFromSource(InputDevice.SOURCE_JOYSTICK) || event.action != MotionEvent.ACTION_MOVE) return false
        for (range in event.device?.motionRanges.orEmpty()) {
            val value = event.getAxisValue(range.axis)
            GamepadMapping.bindForTrigger(range.axis)?.let { pad.set(it, value) }
            GamepadMapping.bindsForAxis(range.axis)?.let { binds ->
                GamepadMapping.axisValues(binds, value, range.flat).forEach { (bind, part) -> pad.set(bind, part) }
            }
        }
        return true
    }
}
