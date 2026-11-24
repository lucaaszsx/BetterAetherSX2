# Landscape

External facts that shape the project, checked on 2026-11-24. Re-check before relying on any of them for a release.

## PCSX2 upstream

- Repository: [PCSX2/pcsx2](https://github.com/PCSX2/pcsx2), GPL-3.0, latest release v2.8.2 (2026-09-04), active daily.
- C++20 (`cmake/BuildParameters.cmake`: `CMAKE_CXX_STANDARD 20`).
- ARM64 builds exist (macOS, Windows on ARM, Linux) but run the CPU **interpreters only**. `pcsx2/arm64/RecStubs.cpp` stubs the recompiler entry points (`pxFailRel("Not implemented.")`).
- ARM64 code that does exist upstream: the GS software renderer JIT (`GSDrawScanlineCodeGenerator.arm64.cpp`, `GSSetupPrimCodeGenerator.arm64.cpp`), the VIF unpack dynarec in NEON, and NEON versions of `GSVector4`/`GSVector4i`.
- Code emitter: upstream already vendors [VIXL](https://github.com/Linaro/vixl) (BSD-3-Clause) in `3rdparty/vixl` for its AArch64 code.
- No Android port upstream.

## ARMSX2

- Repository: [ARMSX2/ARMSX2](https://github.com/ARMSX2/ARMSX2), GPL-3.0, a PCSX2 fork. Latest release 2.9.5 (2026-11-16), active daily.
- Its README states that EE, IOP, VU0/VU1 (microVU) recompilers and vtlb fastmem for ARM64 are complete, with a unit test framework. The tree has them in `pcsx2/arm64/` (`iR5900-arm64.cpp`, `iR3000A-arm64.cpp`, `microVU-arm64.cpp`, `recVTLB-arm64.cpp`, plus a VU ahead-of-time compiler in `pcsx2/arm64/vuaot/`).
- Android app in `platforms/android/` with its own copies of SDL3, Oboe, libzip, freetype, shaderc and other libraries.
- Includes network features: Discord bridge, remote play, UPnP and online play.
- The README says the team uses LLMs in development.
- The "Refresh" Android build (June 2026) moved to native ARM64 code on a PCSX2 v2.7 base ([Android Authority](https://www.androidauthority.com/armsx2-refresh-ps2-emulator-android-3676638/)). Earlier builds were reported slower than NetherSX2.
- Performance against NetherSX2: **not measured**. No controlled comparison is published; reviews are anecdotal and game-dependent.

## NetherSX2

- [Trixarian/NetherSX2-patch](https://github.com/Trixarian/NetherSX2-patch): patches applied to the closed AetherSX2 build 4248. The patch scripts are Unlicense; the emulator itself is closed source. Latest release 2.2n (2025-12-25).
- Still the common reference for speed on low and mid-range devices in reviews.

## Distribution rules

### Android developer verification

From [developer.android.com/developer-verification](https://developer.android.com/developer-verification):

- August 2026: developer APIs, limited-distribution accounts (up to 20 devices, no ID or fee) and the "advanced flow" for power users launched.
- 2026-09-30: enforcement began for apps installed from participating stores in Brazil, Indonesia, Singapore and Thailand, on certified devices with Android 8 or newer.
- 2027: global expansion to all apps on certified devices.
- Unregistered apps can still be installed through the advanced flow (a one-time device setup) or ADB.
- Open-source apps can be [registered](https://developer.android.com/developer-verification/guides/open-source-app-registration) by package name and signing-key fingerprint, including keys used by repositories that re-sign (F-Droid). This needs a verified Android Developer Console account.
- F-Droid [opposes the program](https://f-droid.org/en/2026/02/24/open-letter-opposing-developer-verification.html) and advises developers not to sign up.

Consequence: users in Brazil are already affected if the app is installed from a participating store. Whether to register the package is an owner decision.

### F-Droid inclusion policy

From [f-droid.org/docs/Inclusion_Policy](https://f-droid.org/docs/Inclusion_Policy/):

- Built from source by F-Droid with a fully free toolchain. Prebuilt binaries only from trusted sources (Debian, Maven Central, Google Maven, Android SDK).
- No proprietary libraries (Play Services, Firebase, Crashlytics).
- No downloading of executable code without explicit opt-in. Driver packages picked by the user from local storage fit this; nothing is downloaded.
- Forks need their own application ID, name and icon.
- Bundled fonts and artwork need free licenses.
- Usually at least three months of active development before inclusion.

## Toolchain (current stable)

| Tool | Version | Source |
| --- | --- | --- |
| Android NDK | r30 LTS (30.0.16248370) | [NDK downloads](https://developer.android.com/ndk/downloads) |
| Android Gradle Plugin | 9.5.0 (October 2026); 9.6.0 in preview | [AGP release notes](https://developer.android.com/build/releases/gradle-plugin) |

Gradle, Kotlin, Compose and CMake versions are pinned when the Android project is created, from the AGP compatibility table.
