# Open questions

Ambiguities found during the work, each with the default followed until it is settled.

| ID | Question | Default followed | Status |
| --- | --- | --- | --- |
| OQ-001 | Which C++ standard does upstream PCSX2 use today? Ours must match. | C++20 in `core/CMakeLists.txt` and `.clang-format`. Confirm when PCSX2 is vendored. | Open |
| OQ-002 | CI actions should be pinned by commit SHA. | Major tags of official GitHub actions until dependency versions are pinned. | Open |
| OQ-003 | Copyright holder line for `NOTICE` and source headers. | "Better AetherSX2 contributors". | Open |
| OQ-004 | CI installs the distribution clang-format, which can be older than a contributor's and format some code differently. | The Ubuntu 24.04 package, until one clang-format version is pinned. | Open |
