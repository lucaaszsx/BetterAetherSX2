package io.github.lucaaszsx.betteraethersx2.input

import org.junit.Assert.assertEquals
import org.junit.Test

class PadStateTest {
    @Test
    fun forwardsOnlyChangedValues() {
        val sent = mutableListOf<Pair<PadBind, Float>>()
        val pad = PadState { bind, value -> sent += bind to value }
        pad.set(PadBind.CROSS, 1f)
        pad.set(PadBind.CROSS, 1f)
        pad.set(PadBind.CROSS, 0f)
        assertEquals(listOf(PadBind.CROSS to 1f, PadBind.CROSS to 0f), sent)
    }
}
