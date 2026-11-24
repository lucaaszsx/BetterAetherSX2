package io.github.lucaaszsx.betteraethersx2.runtime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.InputStream

class CoreResourcesTest {
    @get:Rule
    val folder = TemporaryFolder()

    private class MapSource(
        private val files: Map<String, String>,
    ) : ResourceSource {
        var opened = 0

        override fun list(path: String): Array<String> =
            files.keys
                .filter { it.startsWith("$path/") }
                .map { it.removePrefix("$path/").substringBefore('/') }
                .distinct()
                .toTypedArray()

        override fun open(path: String): InputStream {
            opened++
            return files.getValue(path).byteInputStream()
        }
    }

    private val files =
        mapOf(
            "resources/GameIndex.yaml" to "games",
            "resources/shaders/vulkan/tfx.glsl" to "shader",
        )

    @Test
    fun copiesTheTreeIntoTheTarget() {
        val target = folder.root.resolve("resources")

        CoreResources(MapSource(files), target).install(installStamp = 1)

        assertEquals("games", target.resolve("GameIndex.yaml").readText())
        assertEquals("shader", target.resolve("shaders/vulkan/tfx.glsl").readText())
    }

    @Test
    fun skipsTheCopyForTheSameInstall() {
        val target = folder.root.resolve("resources")
        val source = MapSource(files)
        CoreResources(source, target).install(installStamp = 1)

        CoreResources(source, target).install(installStamp = 1)

        assertEquals(files.size, source.opened)
    }

    @Test
    fun replacesAStaleCopyAfterAnUpdate() {
        val target = folder.root.resolve("resources")
        CoreResources(MapSource(files + ("resources/old.txt" to "old")), target).install(installStamp = 1)

        CoreResources(MapSource(files), target).install(installStamp = 2)

        assertFalse(target.resolve("old.txt").exists())
    }
}
