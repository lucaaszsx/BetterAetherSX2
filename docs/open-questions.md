# Open questions

Ambiguities found during the work, each with the default followed until it is settled.

| ID | Question | Default followed | Status |
| --- | --- | --- | --- |
| OQ-001 | Which C++ standard does upstream PCSX2 use today? Ours must match. | C++20. Confirmed in `docs/landscape.md`. | Resolved |
| OQ-002 | CI actions should be pinned by commit SHA. | Major tags of official GitHub actions until dependency versions are pinned. | Open |
| OQ-003 | Copyright holder line for `NOTICE` and source headers. | "Better AetherSX2 contributors". | Open |
| OQ-004 | CI installs the distribution clang-format, which can be older than a contributor's and format some code differently. | The Ubuntu 24.04 package, until one clang-format version is pinned. | Open |
| OQ-005 | PCSX2 picks a compile-time page size; Android devices use 4 KB or 16 KB pages. | Upstream's ARM64 default of 16 KB, which is also correct on 4 KB devices (coarser protection granularity). Revisit if fastmem needs 4 KB granularity. | Open |
| OQ-006 | The Android build cannot create DEV9 network adapters, but the socket adapter and the achievements client are still compiled (unreachable, and the app has no INTERNET permission). | Leave them compiled to keep the upstream patches small. Strip them only if a review of the APK requires it. | Open |
