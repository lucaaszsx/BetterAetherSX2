# Better AetherSX2

A standalone PS2 emulator for Android, built on PCSX2. Install it, import your own BIOS and games, and play with settings tuned for your device.

Status: early foundation work. Nothing is playable yet.

- Android 10 or newer, `arm64-v8a`, Vulkan 1.1.
- Free software under GPL-3.0-or-later. No closed binaries.
- Fully offline: no accounts, no telemetry, no update checks, no network permission.
- Distributed as an APK through GitHub Releases and F-Droid.

## Legal

Better AetherSX2 is not affiliated with Sony Interactive Entertainment or with the developer of the original AetherSX2, and contains none of its code. "PS2" is used only to describe the hardware being emulated.

This project ships no BIOS files, games, ROMs or keys, and does not link to them. You need a BIOS dumped from your own console and games dumped from discs you own.

### Takedown policy

If you believe something in this repository infringes your rights, open an issue titled "Takedown request" with the path of the file and the basis of the claim. The file is removed from the default branch while the claim is reviewed.

## Building

Native core on a Linux host (needs CMake, Ninja, a C++20 compiler and GoogleTest):

```sh
cmake -S core -B build/core -G Ninja
cmake --build build/core
ctest --test-dir build/core
```

The Android app is not in the repository yet.

## Contributing

Read [AGENTS.md](AGENTS.md) first. The repository map is in [docs/architecture.md](docs/architecture.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
