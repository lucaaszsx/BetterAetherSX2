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
    fun keepsOnlyTheFirstParagraph() {
        val reason = "PCSX2 requires a PlayStation 2 BIOS in order to run.\n\nFor legal reasons, obtain it yourself."
        assertEquals("PCSX2 requires a PlayStation 2 BIOS in order to run.", asSentence(reason))
    }

    @Test
    fun hasNoSentenceForAnEmptyReason() {
        assertNull(asSentence(" "))
    }
}
