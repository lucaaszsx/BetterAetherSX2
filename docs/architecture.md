# Architecture

What lives where and why. Recorded in [ADR 0001](adr/0001-repository-layout.md).

## Repository map

| Path | What lives there | Why it is separate |
| --- | --- | --- |
| `core/` | Our native C++ code: runtime, AArch64 recompilers, Vulkan GS work, platform-neutral services. Built with CMake. | The core must build and run its tests on a Linux host with no device, which differential testing of the JIT needs. |
| `upstream/pcsx2/` | Vendored PCSX2, unchanged except for small, isolated patches. Added when PCSX2 is vendored. | Keeping upstream apart from our code makes each rebase mechanical. |
| `android/` | Gradle project: the Kotlin/Compose app and the JNI glue that links the core. Added with the Android app. | The app and the native core change for different reasons and build with different tools. |
| `docs/` | This page, ADRs, design guides, open questions. | |

## Dependency direction

`android/` depends on `core/`. `core/` depends on `upstream/pcsx2/`. Nothing depends on `android/`, and `upstream/pcsx2/` depends on nothing of ours.

## Where code goes

- Runs per frame (CPU emulation, JIT, GS, SPU2, input during play): `core/`, native only.
- Calls into Android APIs from native code (ADPF, Thermal, Oboe, `ANativeWindow`): `android/`, in the JNI glue. The core gets an interface for one of these only when a host test needs a fake.
- Menus, settings, library: `android/`, Kotlin with Compose.
