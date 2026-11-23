#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ndk=${ANDROID_NDK:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/28.2.13676358}}
llvm="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
work="$repo/build/scratch/android-frame-watch"
serial=${1:-}
adb=${ADB:-adb}
if [ -n "$serial" ]; then
    abis=$("$adb" -s "$serial" shell getprop ro.product.cpu.abilist | tr -d '\r')
    remote=$("$adb" -s "$serial" shell mktemp -d /data/local/tmp/kryon-frame-watch.XXXXXXXX | tr -d '\r')
    case "$remote" in /data/local/tmp/kryon-frame-watch.*) ;; *) exit 1;; esac
    trap '"$adb" -s "$serial" shell rm -r "$remote" >/dev/null 2>&1 || true' EXIT
fi
ran=0
for architecture in aarch64 arm x86_64 i386; do
    case "$architecture" in
        aarch64) target=aarch64-linux-android21; cpu=arm64; abi=arm64-v8a;;
        arm) target=armv7a-linux-androideabi21; cpu=none; abi=armeabi-v7a;;
        x86_64) target=x86_64-linux-android21; cpu=x64; abi=x86_64;;
        i386) target=i686-linux-android21; cpu=none; abi=x86;;
    esac
    output="$work/$architecture"
    "$(dirname "$ziran")/zi2c" --target-os android --target-cpu "$cpu" \
        --root "$repo/tests" --module-path "$repo/src/backend" \
        --define ANDROID_BUILD --define ANDROID_DEBUG --define "__${architecture}__" \
        --entry android_frame_watch_test:main --prune-stale -o "$output" \
        "$repo/tests/android_frame_watch_test.zi"
    "$llvm/$target-clang" -std=c11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$ziran_root/include" -I"$output" "$output"/*.c -ldl -o "$output/frame-watch-test"
    printf '%s\n' "$abi frame watch compiled and linked"
    if [ -z "$serial" ]; then continue; fi
    case ",$abis," in *","$abi","*) ;; *) continue;; esac
    "$adb" -s "$serial" push "$output/frame-watch-test" "$remote/frame-watch-test"
    "$adb" -s "$serial" shell chmod 700 "$remote/frame-watch-test"
    "$adb" -s "$serial" shell "$remote/frame-watch-test"
    ran=1
    printf '%s\n' "$abi frame watch sampled a stall, recovered, and preserved signal actions"
done
test -z "$serial" || test "$ran" = 1
