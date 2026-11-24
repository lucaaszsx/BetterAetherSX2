# AGENTS.md

How to work in this repository, for humans and agents.

## Basics

- Everything (docs, code, comments, commits, PRs) is in English. The app UI ships in English and Brazilian Portuguese.
- Decisions that cross areas (module boundaries, data formats, persistence) are explained in the PR that makes them.
- When something is ambiguous, follow the simplest reasonable default and say which one in the PR.

## Repository map

| Path | Contents |
| --- | --- |
| `core/` | Our native C++ code. Builds and tests on a Linux host. |
| `upstream/pcsx2/` | PCSX2 submodule, pinned to a release. Never edit it in place. |
| `upstream/patches/` | Our changes to PCSX2 as patch files, applied by `upstream/apply-patches.sh`. Keep them few and small. |
| `android/` | Gradle project. `app/src/main/kotlin`: Kotlin/Compose UI. `app/src/main/cpp`: the native library `libbax.so` (JNI entry points and the host callbacks PCSX2 expects from a frontend). `deps/build-deps.sh`: builds PCSX2's libraries with the NDK. |

`android/` depends on `core/` and `upstream/`; `core/` depends on `upstream/`; nothing depends on `android/`.

- Runs per frame (CPU emulation, JIT, GS, SPU2, input during play): native code in `core/` or upstream.
- Calls into Android APIs from native code (ADPF, Thermal, audio, `ANativeWindow`): the JNI glue in `android/`.
- Menus, settings, library: Kotlin with Compose in `android/`. The UI reaches the emulator only through `EmulatorRuntime`; `FakeRuntime` stands in for the core in UI work and tests.

## Commands

```sh
cmake -S core -B build/core -G Ninja && cmake --build build/core && ctest --test-dir build/core
clang-format --dry-run --Werror $(git ls-files 'core/*.cpp' 'core/*.h')
cd android && ./gradlew ktlintCheck detekt testDebugUnitTest assembleDebug
```

Keep the local loop fast. Heavy suites (full differential runs, fuzzing, sanitizers, GS golden tests) run in CI; device tests and benchmarks run on real devices.

## Code

- Clean and direct: intention-revealing names, small functions, no dead or commented-out code, no magic numbers, never swallow an error, comments explain why.
- No module, layer, interface or factory without a concrete reason (a second implementation, a test need, a build boundary). Say the reason in the PR. Deleting an unneeded abstraction is a good change.
- The same applies to tooling: use the standard formatters and linters. Do not add custom check scripts.
- Boundaries that stay: the core is separate from the app, the core builds and tests without a device, upstream code is separate from ours.
- Everything that runs per frame is native code. Kotlin never sits on the per-frame path.
- 4 spaces, never tabs. Comments in English.
- Single quotes wherever the language allows (Python, shell, JS/TS); C++, Kotlin and JSON use double quotes for strings.
- A block with a single statement goes on the same line, without braces: `if (!ready) return nullptr;`

## UI copy and visuals

- Copy says what the thing does or what happened, in plain, short words. Buttons are a verb plus an object. Errors say what happened, why, and what to do next. Progress shows real numbers.
- Avoid filler and marketing voice: exclamation marks, cheering, greetings, emoji, words like "seamless" or "unleash" used for effect.
- No sparkle or "magic" motifs, purple-pink gradients, gradient text, neon glows, blob backgrounds, stock people or robots, or untouched stock Material components.
- Example: write "Couldn't read this BIOS file. PS2 BIOS dumps are 4 MB and this file is 3 MB. Choose a different file.", not "Oops! Something went wrong."

## Workflow

1. Write a short plan in the draft PR description.
2. Keep PRs small: at most 400 changed lines, not counting generated or vendored code.
3. Tests come with the code. Run the formatter and the fast local suite.
4. Someone other than the author reviews. Merge only with green CI.
5. Update `AGENTS.md` and `README.md` in the same PR when the change affects them.

Conventional Commits. `main` is protected. Branches: `feat/...`, `fix/...`.

## Never

- Commit BIOS files, games, keys, commercial game art or any copyrighted file, or link to where to get them.
- Add network access of any kind, including the INTERNET permission.
- Add closed-source binaries or dependencies whose license is incompatible with GPL-3.0-or-later. Record each new dependency's license in `NOTICE`.
- Invent benchmark numbers, test results or compatibility ratings. Without evidence, write "not measured".
- Disable or weaken tests to make CI pass.
- Leave a TODO without an issue ID.
