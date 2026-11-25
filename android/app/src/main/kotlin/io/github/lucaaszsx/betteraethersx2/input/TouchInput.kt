package io.github.lucaaszsx.betteraethersx2.input

import android.view.MotionEvent

// What a finger did. The order matches TouchAction in the native TouchControls.cpp.
enum class TouchAction {
    DOWN,
    MOVE,
    UP,
}

// Splits a MotionEvent into one call per finger, the shape the native touch controls take.
class TouchInput(
    private val send: (action: TouchAction, pointerId: Int, x: Float, y: Float) -> Unit,
) {
    fun onTouch(event: MotionEvent) {
        val changed = event.actionIndex
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> sendPointer(event, changed, TouchAction.DOWN)

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> sendPointer(event, changed, TouchAction.UP)

            MotionEvent.ACTION_MOVE -> repeat(event.pointerCount) { sendPointer(event, it, TouchAction.MOVE) }

            // The system took the gesture away, for example for a system bar swipe: every finger is lifted.
            MotionEvent.ACTION_CANCEL -> repeat(event.pointerCount) { sendPointer(event, it, TouchAction.UP) }
        }
    }

    private fun sendPointer(
        event: MotionEvent,
        index: Int,
        action: TouchAction,
    ) = send(action, event.getPointerId(index), event.getX(index), event.getY(index))
}
