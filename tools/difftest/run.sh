#!/usr/bin/env bash
# Runs the EE differential tester on an Android device over adb.
#
#   tools/difftest/run.sh <app.apk> <bax_difftest> <class> <cases> [seed]
#
# The tester links the same shared libraries as the app, so they are taken from the APK and pushed next to it.

set -euo pipefail

if [ $# -lt 4 ]; then
    sed -n '2,6p' "$0"
    exit 2
fi
apk=$1
tester=$2
shift 2

readonly DEVICE_DIR='/data/local/tmp/bax-difftest'
libraries=$(mktemp -d)
trap 'rm -rf "$libraries"' EXIT

unzip -q -j "$apk" 'lib/arm64-v8a/*.so' -d "$libraries"
adb shell rm -rf "$DEVICE_DIR"
adb shell mkdir -p "$DEVICE_DIR"
adb push -q "$libraries"/*.so "$tester" "$DEVICE_DIR/" > /dev/null
adb shell chmod 755 "$DEVICE_DIR/bax_difftest"
adb shell "cd $DEVICE_DIR && LD_LIBRARY_PATH=. ./bax_difftest $*"
