package io.github.lucaaszsx.betteraethersx2.input

// Remembers the last value of each bind and forwards only changes, so a controller that repeats the same report
// does not flood the core with events.
class PadState(
    private val send: (PadBind, Float) -> Unit,
) {
    private val values = FloatArray(PadBind.entries.size)

    fun set(
        bind: PadBind,
        value: Float,
    ) {
        if (values[bind.ordinal] == value) return
        values[bind.ordinal] = value
        send(bind, value)
    }
}
