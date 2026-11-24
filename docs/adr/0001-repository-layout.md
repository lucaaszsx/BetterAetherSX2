# ADR 0001: Repository layout

- Status: Accepted
- Date: 2026-11-24

## Context

Three boundaries are fixed: the native core apart from the Android app, a core that builds and tests on a Linux host, and upstream PCSX2 apart from our code. The rest of the layout is open.

## Decision

One repository with top-level `core/`, `upstream/pcsx2/`, `android/` and `docs/`, as described in [architecture.md](../architecture.md). Each top-level directory matches one of the boundaries. No further split (no per-subsystem repositories, no shared "common" module) until a concrete need appears.

`upstream/pcsx2/` and `android/` are created when their content arrives, not committed empty. How PCSX2 is vendored (subtree, submodule or fork) is decided when it is vendored.

## Consequences

- One CI pipeline covers everything; cross-area changes land in one PR.
- `core/` never includes Android headers outside code guarded for the Android build.
- A rebase on upstream touches only `upstream/`.
