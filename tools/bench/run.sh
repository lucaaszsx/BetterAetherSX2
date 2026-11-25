#!/usr/bin/env bash
# Measures a game run on an Android device over adb, the same way for this app and for other emulators.
#
#   tools/bench/run.sh app <label> <game-uri> [seconds] [warmup]
#       Opens the game in a debug build of this app with benchmark logging on.
#   tools/bench/run.sh other <label> <package> [seconds] [warmup]
#       Another emulator: start the game on the device by hand, then press Enter.
#
# Frame timing comes from SurfaceFlinger's timestats for the app's layers, so every emulator is measured alike. This
# app also logs the core's own metrics (speed, thread usage). Needs adb and jq.
#
# Results land in bench/out/<time>-<label>/: the raw dumps and summary.json, where a field that is not measured is null.
# Runs worth keeping, such as baselines, are committed to the bench-results branch.

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

# adb shell hands its arguments to the device's shell as one string. Game URIs keep ' ( ) unencoded, so quote them.
quote_for_device() {
    local escaped=${1//\'/\'\\\'\'}
    printf "'%s'" "$escaped"
}

case $mode in
    app)
        package=$APP_PACKAGE
        adb shell am force-stop "$package"
        adb shell am start -n "$package/.GameActivity" \
            --es "$package.GAME" "$(quote_for_device "$target")" --ez "$package.BENCHMARK" true > /dev/null
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
    # Current in microamps and voltage in microvolts, once a second. Some devices do not let adb read them.
    adb shell cat "$BATTERY/current_now" "$BATTERY/voltage_now" 2> /dev/null | paste -sd ' ' >> "$out/power.txt" || true
    sleep 1
done

adb shell dumpsys SurfaceFlinger --timestats -dump > "$out/timestats.txt"
adb shell dumpsys SurfaceFlinger --timestats -disable > /dev/null
adb shell dumpsys thermalservice > "$out/thermal.txt"
adb logcat -d -s BaxBench:I -v raw | grep '^{' > "$out/metrics.jsonl" || true
# Peak resident memory of the process, in kB.
adb shell "grep VmHWM /proc/\$(pidof -s $package)/status" > "$out/memory.txt" || true
if [ "$mode" = app ]; then adb shell input keyevent KEYCODE_BACK; fi

# Frame pacing of the package's busiest layer (the game surface), from its present2present histogram ("<ms>ms=<count>"
# pairs): frame count, average FPS, p50/p95/p99 of the time between presents, and stutters (presents over 50 ms).
frames=$(awk -v package="$package" '
    /layerName = / { ours = index($0, package) > 0 }
    ours && /averageFPS = / { fps = $3 }
    ours && /present2present histogram/ { histogram = 1; next }
    histogram {
        histogram = 0
        total = 0
        stutters = 0
        for (i = 1; i <= NF; i++) {
            split($i, pair, "ms=")
            ms[i] = pair[1]
            count[i] = pair[2]
            total += pair[2]
            if (pair[1] > 50) stutters += pair[2]
        }
        if (total <= best) next
        best = total
        line = "\"frames\":" total ",\"avgFps\":" fps ",\"stutters\":" stutters
        split("p50 p95 p99", name, " ")
        split("0.50 0.95 0.99", fraction, " ")
        for (p = 1; p <= 3; p++) {
            seen = 0
            for (i = 1; i <= NF; i++) { seen += count[i]; if (seen >= fraction[p] * total) break }
            line = line ",\"" name[p] "Ms\":" ms[i]
        }
    }
    END { print (best > 0 ? "{" line "}" : "null") }
' "$out/timestats.txt")

# Average power in mW from current (uA, signed differently per device) and voltage (uV). It only means the run's draw
# when the device is not charging.
power=$(awk 'NF == 2 { current = $1 < 0 ? -$1 : $1; sum += current * $2 / 1e9; n++ }
    END { print (n ? sum / n : "null") }' "$out/power.txt")
charging=$(adb shell dumpsys battery | grep -q 'powered: true' && echo true || echo false)
# Highest CPU and skin temperatures at the end of the run, from the HAL's current readings.
temperature() {
    grep -o "mValue=[0-9.]*, mType=$1," "$out/thermal.txt" | cut -d= -f2 | cut -d, -f1 | sort -n | tail -n 1 || true
}
cpu_temperature=$(temperature 0)
skin_temperature=$(temperature 3)
peak_ram=$(grep -o '[0-9]*' "$out/memory.txt" || true)
version=$(adb shell dumpsys package "$package" | grep -m 1 -o 'versionName=[^ ]*' | cut -d= -f2 || true)

# commit is this repository's HEAD, which in app mode should be the build that is installed.
jq -n \
    --arg label "$label" --arg mode "$mode" --arg package "$package" --arg version "$version" \
    --arg commit "$(git rev-parse HEAD)" --arg target "$target" \
    --arg device "$(adb shell getprop ro.product.model)" --arg android "$(adb shell getprop ro.build.version.release)" \
    --argjson seconds "$seconds" --argjson warmup "$warmup" --argjson frames "$frames" --argjson powerMw "$power" \
    --argjson charging "$charging" \
    --argjson cpuC "${cpu_temperature:-null}" --argjson skinC "${skin_temperature:-null}" \
    --argjson peakRamKb "${peak_ram:-null}" \
    --slurpfile core "$out/metrics.jsonl" '
    {
        label: $label, mode: $mode, package: $package, version: $version, commit: $commit, target: $target,
        device: $device, android: $android, seconds: $seconds, warmup: $warmup,
        frames: $frames, powerMw: $powerMw, charging: $charging, temperatureC: { cpu: $cpuC, skin: $skinC },
        peakRamKb: $peakRamKb,
        cpuUse: null, gpuUse: null,
        core: (if ($core | length) > 0 then
            [$core[] | to_entries[]] | group_by(.key) | map({ key: .[0].key, value: (map(.value) | add / length) })
            | from_entries + { samples: ($core | length) }
        else null end)
    }' | tee "$out/summary.json"
echo "Results in $out"
