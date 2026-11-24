plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.ktlint)
    alias(libs.plugins.detekt)
}

val repoRoot: File = rootProject.projectDir.parentFile

// Where android/deps/build-deps.sh installed PCSX2's libraries. Override with -Pbax.depsDir=<path>.
val depsDir: File =
    providers
        .gradleProperty("bax.depsDir")
        .map { file(it) }
        .getOrElse(repoRoot.resolve("build/android-deps"))

android {
    // The package id lives only here and the app name only in strings.xml, so a rename touches two lines.
    namespace = "io.github.lucaaszsx.betteraethersx2"
    // Compose 1.13 needs the 37.1 or newer SDK extension level.
    compileSdk {
        version = release(37) { minorApiLevel = 2 }
    }

    defaultConfig {
        applicationId = "io.github.lucaaszsx.betteraethersx2"
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }

        externalNativeBuild {
            cmake {
                // Every library shares one libc++, so the dependencies and the app must agree on c++_shared.
                arguments +=
                    listOf(
                        "-DANDROID_STL=c++_shared",
                        "-DPCSX2_EXTERNAL_FRONTEND_DIR=${projectDir.resolve("src/main/cpp")}",
                        "-DCMAKE_PREFIX_PATH=$depsDir",
                        "-DCMAKE_FIND_ROOT_PATH=$depsDir",
                        // Desktop pieces of upstream: the Qt UI, the OpenGL renderer (the app is Vulkan only)
                        // and the unit tests.
                        "-DENABLE_QT_UI=OFF",
                        "-DENABLE_TESTS=OFF",
                        "-DUSE_OPENGL=OFF",
                        "-DUSE_VULKAN=ON",
                    )
                targets += "bax"
            }
        }
    }

    ndkVersion = "30.0.16248370"

    externalNativeBuild {
        cmake { path = repoRoot.resolve("upstream/pcsx2/CMakeLists.txt") }
    }

    signingConfigs {
        // A fixed, public debug key, so a debug APK from any machine or CI run updates the installed one.
        getByName("debug") {
            storeFile = file("debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures { compose = true }
}

val applyUpstreamPatches =
    tasks.register<Exec>("applyUpstreamPatches") {
        commandLine(repoRoot.resolve("upstream/apply-patches.sh"))
    }

// The dependencies' shared libraries ship in the APK next to libbax.so.
val dependencyJniLibs: File =
    layout.buildDirectory
        .dir("dependency-jni-libs")
        .get()
        .asFile

val stageDependencyLibraries =
    tasks.register<Sync>("stageDependencyLibraries") {
        from(depsDir.resolve("lib")) { include("*.so") }
        into(dependencyJniLibs.resolve("arm64-v8a"))
    }

androidComponents {
    onVariants { variant -> variant.sources.jniLibs?.addStaticSourceDirectory(dependencyJniLibs.path) }
}

tasks.configureEach {
    if (name.startsWith("configureCMake")) dependsOn(applyUpstreamPatches)
    if (name.startsWith("merge") && name.endsWith("JniLibFolders")) dependsOn(stageDependencyLibraries)
}

ktlint {
    version.set(libs.versions.ktlint)
}

detekt {
    config.setFrom(rootProject.file("config/detekt.yml"))
    buildUponDefaultConfig = true
}

dependencies {
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.foundation)
    implementation(libs.compose.material3)
    implementation(libs.activity.compose)
    implementation(libs.core.ktx)
    implementation(libs.coroutines.android)
    // SDL's native library registers its Java classes when it loads, so they must be in the APK even though the
    // app never starts SDLActivity. The jar is built from source with the other dependencies.
    implementation(
        fileTree(depsDir.resolve("share/java/SDL3")) {
            include("SDL3-*.jar")
            exclude("*-sources.jar", "*-javadoc.jar")
        },
    )
    testImplementation(libs.junit)
}
