#!/usr/bin/env bash
# Applies our patches to the PCSX2 submodule in order. Patches already applied are skipped, so it is safe to rerun.

set -euo pipefail

UPSTREAM_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

for patch in "$UPSTREAM_DIR"/patches/*.patch; do
    if git -C "$UPSTREAM_DIR/pcsx2" apply --reverse --check "$patch" 2> /dev/null; then
        echo "Already applied: $(basename "$patch")"
        continue
    fi
    git -C "$UPSTREAM_DIR/pcsx2" apply "$patch"
    echo "Applied: $(basename "$patch")"
done
