#!/usr/bin/env bash
# Configures the PCSX2 core for Android arm64-v8a against dependencies built by deps/build-deps.sh.
# Build afterwards with: cmake --build <build-dir> --target common PCSX2
#
# Usage: configure-core.sh <ndk-dir> <deps-prefix> <build-dir>

set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo 'usage: configure-core.sh <ndk-dir> <deps-prefix> <build-dir>' >&2
    exit 2
fi

NDK="$(realpath "$1")"
DEPS="$(realpath "$2")"
BUILD_DIR="$3"
REPO_ROOT="$(realpath "$(dirname "${BASH_SOURCE[0]}")/..")"

"$REPO_ROOT/upstream/apply-patches.sh"

# Upstream options that would pull in desktop pieces: the Qt UI, OpenGL (the app is Vulkan only),
# X11/Wayland windowing and libbacktrace.
cmake -S "$REPO_ROOT/upstream/pcsx2" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=29 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$DEPS" \
    -DCMAKE_FIND_ROOT_PATH="$DEPS" \
    -DENABLE_QT_UI=OFF \
    -DENABLE_TESTS=OFF \
    -DUSE_OPENGL=OFF \
    -DUSE_VULKAN=ON
