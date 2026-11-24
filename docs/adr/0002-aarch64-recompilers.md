# ADR 0002: AArch64 recompilers

- Status: Accepted
- Date: 2026-11-24

## Context

Upstream PCSX2 runs on ARM64 with the CPU interpreters only; its EE, IOP and VU recompilers are x86-64. The ARM64 pieces upstream does have are the GS software-renderer JIT, the VIF unpack dynarec and the NEON vector classes, all built on VIXL. ARMSX2, a GPL-3.0 fork, ships its own ARM64 recompilers. Details in [landscape.md](../landscape.md).

## Decision

We write the EE, IOP and VU0/VU1 recompilers for AArch64 in this project. We do not copy ARMSX2's recompiler code.

- Emitter: VIXL, already vendored by upstream, so no second code generator enters the build.
- Reused from upstream as is: the interpreters (fallback and test oracle), the ARM64 GS software-renderer JIT, the VIF NEON dynarec, `GSVector4`/`GSVector4i` NEON.
- Our recompilers live in `core/`, and plug into upstream where `pcsx2/arm64/RecStubs.cpp` stubs the entry points today. The upstream patch that wires them in stays small.
- A recompiler is enabled only after it passes differential tests against the interpreter.

## Consequences

- The JIT is the longest piece of work; until it passes, builds run on the interpreter and make no speed claims.
- ARMSX2 stays a point of comparison for benchmarks, not a code source for the JIT.
- Code outside the JIT from ARMSX2 may still be reused under GPL-3.0 with credit, case by case.
