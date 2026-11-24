package io.github.lucaaszsx.betteraethersx2.runtime

import java.io.File

// Stands in for the native core in UI work and tests. It never emulates anything.
class FakeRuntime(
    private val bios: BiosIdentity? = BiosIdentity(region = "USA", version = "2.20"),
) : EmulatorRuntime {
    override val coreVersion: String = "0.0.0-fake"

    override fun identifyBios(file: File): BiosIdentity? = bios
}
