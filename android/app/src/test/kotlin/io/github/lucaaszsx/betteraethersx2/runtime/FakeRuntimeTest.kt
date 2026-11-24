package io.github.lucaaszsx.betteraethersx2.runtime

import org.junit.Assert.assertTrue
import org.junit.Test

class FakeRuntimeTest {
    @Test
    fun reportsAFakeVersionSoItIsNeverMistakenForTheRealCore() {
        assertTrue(FakeRuntime().coreVersion.endsWith("-fake"))
    }
}
