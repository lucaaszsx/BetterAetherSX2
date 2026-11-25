package io.github.lucaaszsx.betteraethersx2.input

import android.view.KeyEvent
import android.view.MotionEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GamepadMappingTest {
    private val leftStickX = GamepadMapping.AxisBinds(PadBind.LEFT_STICK_LEFT, PadBind.LEFT_STICK_RIGHT)

    @Test
    fun mapsTheBottomFaceButtonToCross() {
        assertEquals(PadBind.CROSS, GamepadMapping.bindForKey(KeyEvent.KEYCODE_BUTTON_A))
    }

    @Test
    fun ignoresKeysThatAreNotOnAGamepad() {
        assertNull(GamepadMapping.bindForKey(KeyEvent.KEYCODE_VOLUME_UP))
    }

    @Test
    fun mapsTheLeftStickAndHat() {
        assertEquals(leftStickX, GamepadMapping.bindsForAxis(MotionEvent.AXIS_X))
        assertEquals(PadBind.UP, GamepadMapping.bindsForAxis(MotionEvent.AXIS_HAT_Y)?.negative)
    }

    @Test
    fun mapsBothTriggerAxes() {
        assertEquals(PadBind.L2, GamepadMapping.bindForTrigger(MotionEvent.AXIS_BRAKE))
        assertEquals(PadBind.R2, GamepadMapping.bindForTrigger(MotionEvent.AXIS_RTRIGGER))
    }

    @Test
    fun splitsAnAxisIntoItsTwoDirections() {
        assertEquals(
            listOf(PadBind.LEFT_STICK_LEFT to 0.5f, PadBind.LEFT_STICK_RIGHT to 0f),
            GamepadMapping.axisValues(leftStickX, -0.5f, flat = 0.1f),
        )
    }

    @Test
    fun centersAStickInsideItsFlatZone() {
        assertEquals(
            listOf(PadBind.LEFT_STICK_LEFT to 0f, PadBind.LEFT_STICK_RIGHT to 0f),
            GamepadMapping.axisValues(leftStickX, 0.05f, flat = 0.1f),
        )
    }

    @Test
    fun keepsTheCoreIndicesOfTheFirstAndLastBinds() {
        // PadDualshock2::Inputs: PAD_UP is 0, PAD_R_LEFT is 25.
        assertEquals(0, PadBind.UP.ordinal)
        assertEquals(25, PadBind.RIGHT_STICK_LEFT.ordinal)
    }
}
