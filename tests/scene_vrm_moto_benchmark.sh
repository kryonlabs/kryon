#!/bin/sh
# Invoke through build-queue. Build separately from the physical measurement.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
cd "$repo"
out="$repo/build/scratch/scene-vrm-moto"
serial=ZE2223BQZT
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
case "${1:-}" in
    build)
        mkdir -p "$out/c"
        ndk_base=${ANDROID_HOME:-$HOME/Android/Sdk}/ndk
        toolchain=$(ls -d "$ndk_base"/*/toolchains/llvm/prebuilt/linux-x86_64 2>/dev/null | sort | tail -n 1)
        test -n "$toolchain"
        "$ziran" build --project --root "$repo/tests" \
            --target=c --target-os android --target-cpu none \
            --define POSIX_THREADS --define ANDROID_BUILD --define __arm__ \
            --no-main -o "$out/c" "$repo/tests/scene_vrm_benchmark.zi"
        codecs=$(python3 "$repo/tests/android_codec_artifact_test.py" --link armv7)
        bridges=$(python3 "$repo/tests/android_codec_artifact_test.py" --shared armv7)
        # shellcheck disable=SC2086
        printf '%s\n' $bridges > "$out/bridges.txt"
        # Intentional expansion: the existing codec helper returns link paths.
        # shellcheck disable=SC2086
        "$toolchain/bin/armv7a-linux-androideabi26-clang" -std=c11 -O2 \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -I"$ziran_root/include" -iquote "$out/c" "$out/c"/*.c $codecs \
            -lEGL -lGLESv3 -landroid -lmediandk -llog -ldl -lm -lz \
            -o "$out/scene-vrm-benchmark"
        printf 'Native ARMv7 VRM benchmark built; not run on Moto\n'
        ;;
    run)
        test "$#" -eq 3 || { echo 'run LABEL MODEL.vrm' >&2; exit 2; }
        label=$2
        case "$label" in ''|*[!a-z0-9_-]*) echo 'Invalid label' >&2; exit 2;; esac
        model=$3
        test -f "$model"
        test "$(adb -s "$serial" shell getprop ro.product.model | tr -d '\r')" = 'moto e6 play'
        test "$(adb -s "$serial" shell getprop ro.product.cpu.abi | tr -d '\r')" = armeabi-v7a
        log="$repo/backup/figure-preview-lag/moto-$label.log"
        git check-ignore "$log" >/dev/null
        test ! -e "$log"
        sha256sum "$out/scene-vrm-benchmark" "$model" > "$log.identity"
        remote=/data/local/tmp/kryon-scene-vrm-benchmark
        trap 'adb -s "$serial" shell rm -rf "$remote"' EXIT HUP INT TERM
        adb -s "$serial" shell mkdir -p "$remote"
        adb -s "$serial" push "$out/scene-vrm-benchmark" "$remote/probe"
        adb -s "$serial" push "$model" "$remote/model.vrm"
        while IFS= read -r bridge; do
            test -z "$bridge" || adb -s "$serial" push "$bridge" "$remote/"
        done < "$out/bridges.txt"
        adb -s "$serial" shell chmod 700 "$remote/probe"
        adb -s "$serial" shell sha256sum "$remote/probe" "$remote/model.vrm" \
            | tr -d '\r' > "$log.device-identity"
        expected=$(awk '{print $1}' "$log.identity")
        actual=$(awk '{print $1}' "$log.device-identity")
        test "$expected" = "$actual" || {
            echo 'Transferred benchmark or model differs from recorded identity' >&2
            exit 1
        }
        if adb -s "$serial" shell "cd $remote && LD_LIBRARY_PATH=$remote ./probe model.vrm" > "$log"; then
            printf 'Moto raw samples saved: %s\n' "$log"
        else
            tail -n 20 "$log"
            exit 1
        fi
        ;;
    *) echo 'Usage: scene_vrm_moto_benchmark.sh build | run LABEL MODEL.vrm' >&2; exit 2;;
esac
