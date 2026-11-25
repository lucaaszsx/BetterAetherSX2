package io.github.lucaaszsx.betteraethersx2.input

import android.view.KeyEvent
import android.view.MotionEvent
import kotlin.math.abs

// The default layout of an Android gamepad on a DualShock 2: buttons by position, so the bottom face button is Cross
// on every controller.
object GamepadMapping {
    // A stick or hat axis drives two binds, one for each direction.
    data class AxisBinds(
        val negative: PadBind,
        val positive: PadBind,
    )

    private val keyBinds =
        mapOf(
            KeyEvent.KEYCODE_BUTTON_A to PadBind.CROSS,
            KeyEvent.KEYCODE_BUTTON_B to PadBind.CIRCLE,
            KeyEvent.KEYCODE_BUTTON_X to PadBind.SQUARE,
            KeyEvent.KEYCODE_BUTTON_Y to PadBind.TRIANGLE,
            KeyEvent.KEYCODE_BUTTON_L1 to PadBind.L1,
            KeyEvent.KEYCODE_BUTTON_R1 to PadBind.R1,
            KeyEvent.KEYCODE_BUTTON_L2 to PadBind.L2,
            KeyEvent.KEYCODE_BUTTON_R2 to PadBind.R2,
            KeyEvent.KEYCODE_BUTTON_THUMBL to PadBind.L3,
            KeyEvent.KEYCODE_BUTTON_THUMBR to PadBind.R3,
            KeyEvent.KEYCODE_BUTTON_START to PadBind.START,
            KeyEvent.KEYCODE_BUTTON_SELECT to PadBind.SELECT,
            KeyEvent.KEYCODE_DPAD_UP to PadBind.UP,
            KeyEvent.KEYCODE_DPAD_RIGHT to PadBind.RIGHT,
            KeyEvent.KEYCODE_DPAD_DOWN to PadBind.DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT to PadBind.LEFT,
        )

    fun bindForKey(keyCode: Int): PadBind? = keyBinds[keyCode]

    // Android reports up and left as negative values.
    fun bindsForAxis(axis: Int): AxisBinds? =
        when (axis) {
            MotionEvent.AXIS_X -> AxisBinds(PadBind.LEFT_STICK_LEFT, PadBind.LEFT_STICK_RIGHT)
            MotionEvent.AXIS_Y -> AxisBinds(PadBind.LEFT_STICK_UP, PadBind.LEFT_STICK_DOWN)
            MotionEvent.AXIS_Z -> AxisBinds(PadBind.RIGHT_STICK_LEFT, PadBind.RIGHT_STICK_RIGHT)
            MotionEvent.AXIS_RZ -> AxisBinds(PadBind.RIGHT_STICK_UP, PadBind.RIGHT_STICK_DOWN)
            MotionEvent.AXIS_HAT_X -> AxisBinds(PadBind.LEFT, PadBind.RIGHT)
            MotionEvent.AXIS_HAT_Y -> AxisBinds(PadBind.UP, PadBind.DOWN)
            else -> null
        }

    // Analog triggers, which run from 0 to 1. Controllers report them on one axis or the other.
    fun bindForTrigger(axis: Int): PadBind? =
        when (axis) {
            MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE -> PadBind.L2
            MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS -> PadBind.R2
            else -> null
        }

    // Splits an axis value into its two direction binds. Values inside the controller's flat zone count as centered,
    // so a resting stick sends nothing.
    fun axisValues(
        binds: AxisBinds,
        value: Float,
        flat: Float,
    ): List<Pair<PadBind, Float>> {
        val live = if (abs(value) <= flat) 0f else value
        // Written out rather than with coerceAtLeast, which would turn a centered axis into -0 on the negative side.
        val negative = if (live < 0f) -live else 0f
        val positive = if (live > 0f) live else 0f
        return listOf(binds.negative to negative, binds.positive to positive)
    }
}
