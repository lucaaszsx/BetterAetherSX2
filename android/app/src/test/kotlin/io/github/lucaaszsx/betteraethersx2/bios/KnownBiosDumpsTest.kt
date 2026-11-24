package io.github.lucaaszsx.betteraethersx2.bios

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class KnownBiosDumpsTest {
    private val list =
        """
        # comment
        AEA061E6E263FDCC1C4FDBD68553EF78DAE74263 SCPH-10000 (Version 5.0 01/17/00 T)

        """.trimIndent()

    @Test
    fun findsADumpByHashIgnoringCase() {
        val dumps = KnownBiosDumps.parse(list.byteInputStream())

        assertEquals("SCPH-10000 (Version 5.0 01/17/00 T)", dumps.nameOf("aea061e6e263fdcc1c4fdbd68553ef78dae74263"))
    }

    @Test
    fun returnsNullForAnUnknownHash() {
        val dumps = KnownBiosDumps.parse(list.byteInputStream())

        assertNull(dumps.nameOf("0000000000000000000000000000000000000000"))
    }
}
