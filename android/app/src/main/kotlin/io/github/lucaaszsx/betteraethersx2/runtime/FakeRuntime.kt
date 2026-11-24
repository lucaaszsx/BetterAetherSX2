package io.github.lucaaszsx.betteraethersx2.runtime

// Stands in for the native core in UI work and tests. It never emulates anything.
class FakeRuntime : EmulatorRuntime {
    override val coreVersion: String = "0.0.0-fake"
}
