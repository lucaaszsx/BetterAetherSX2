#!/usr/bin/env bash
# Measures a game run on an Android device over adb, the same way for this app and for other emulators.
#
#   tools/bench/run.sh app <label> <game-uri> [seconds] [warmup]
#       Opens the game in a debug build of this app with benchmark logging on.
#   tools/bench/run.sh other <label> <package> [seconds] [warmup]
#       Another emulator: start the game on the device by hand, then press Enter.
#
# Frame timing comes from SurfaceFlinger's timestats for the app's layers, so every emulator is measured alike. This
# app also logs the core's own metrics (speed, thread usage). Results land in bench/out/<time>-<label>/.

set -euo pipefail

readonly APP_PACKAGE='io.github.lucaaszsx.betteraethersx2'
readonly BATTERY='/sys/class/power_supply/battery'

if [ $# -lt 3 ]; then
    sed -n '2,10p' "$0"
    exit 2
fi
mode=$1
label=$2
target=$3
seconds=${4:-60}
warmup=${5:-20}
out="bench/out/$(date +%Y%m%d-%H%M%S)-$label"
mkdir -p "$out"

case $mode in
    app)
        package=$APP_PACKAGE
        adb shell am force-stop "$package"
        adb shell am start -n "$package/.GameActivity" \
            --es "$package.GAME" "$target" --ez "$package.BENCHMARK" true > /dev/null
        ;;
    other)
        package=$target
        read -r -p 'Start the game on the device, then press Enter. ' _
        ;;
    *)
        echo "Unknown mode '$mode'; use app or other." >&2
        exit 2
        ;;
esac

echo "Warming up for ${warmup}s"
sleep "$warmup"
adb logcat -c
adb shell dumpsys SurfaceFlinger --timestats -enable -clear > /dev/null

echo "Measuring for ${seconds}s"
for _ in $(seq "$seconds"); do
    # Current in microamps and voltage in microvolts, once a second.
    adb shell cat "$BATTERY/current_now" "$BATTERY/voltage_now" | paste -sd ' ' >> "$out/power.txt"
    sleep 1
done

adb shell dumpsys SurfaceFlinger --timestats -dump > "$out/timestats.txt"
adb shell dumpsys SurfaceFlinger --timestats -disable > /dev/null
adb shell dumpsys thermalservice > "$out/thermal.txt"
adb logcat -d -s BaxBench:I -v raw | grep '^{' > "$out/metrics.jsonl" || true
if [ "$mode" = app ]; then adb shell input keyevent KEYCODE_BACK; fi

# Frame pacing per layer of the package: frame count, average FPS, and percentiles of the time between presents,
# read from the present2present histogram ("<ms>ms=<count>" pairs).
awk -v package="$package" '
    /layerName = / { layer = $0; ours = index($0, package) > 0 }
    ours && /totalFrames = |droppedFrames = |averageFPS = / { print layer; print "  " $0 }
    ours && /present2present histogram/ { histogram = 1; next }
    histogram {
        histogram = 0
        total = 0
        for (i = 1; i <= NF; i++) { split($i, pair, "ms="); ms[i] = pair[1]; count[i] = pair[2]; total += pair[2] }
        if (total == 0) next
        line = "  present2present p50/p95/p99 ms:"
        for (p = 1; p <= 3; p++) {
            target = (p == 1 ? 0.50 : p == 2 ? 0.95 : 0.99) * total
            seen = 0
            for (i = 1; i <= NF; i++) { seen += count[i]; if (seen >= target) { line = line " " ms[i]; break } }
        }
        print line
    }
' "$out/timestats.txt" | uniq | tee "$out/summary.txt"

if [ -s "$out/metrics.jsonl" ]; then
    awk -F'[:,}]' '{ fps += $2; speed += $4; ee += $12; n++ }
        END { printf "core: fps %.1f, speed %.1f%%, EE thread %.1f%% over %d samples\n", fps / n, speed / n, ee / n, n }' \
        "$out/metrics.jsonl" | tee -a "$out/summary.txt"
fi
echo "Results in $out"
