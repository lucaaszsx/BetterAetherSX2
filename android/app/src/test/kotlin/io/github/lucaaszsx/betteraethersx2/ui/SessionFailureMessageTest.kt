package io.github.lucaaszsx.betteraethersx2.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SessionFailureMessageTest {
    @Test
    fun endsACoreFragmentWithAPeriod() {
        assertEquals("Failed to load BIOS.", asSentence("Failed to load BIOS"))
    }

    @Test
    fun keepsTheCoresOwnPunctuationAndTrimsSpace() {
        assertEquals("No BIOS found!", asSentence("  No BIOS found!\n"))
    }

    @Test
    fun hasNoSentenceForAnEmptyReason() {
        assertNull(asSentence(" "))
    }
}
