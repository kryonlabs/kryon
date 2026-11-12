#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/shader-android"
mkdir -p "$out"
ndk_base=${ANDROID_HOME:-$HOME/Android/Sdk}/ndk
toolchain=$(ls -d "$ndk_base"/*/toolchains/llvm/prebuilt/linux-x86_64 2>/dev/null | sort | tail -n 1 || true)
if [ -z "$toolchain" ]; then echo 'Android shader host test requires NDK' >&2; exit 1; fi
# This is an explicit physical-device test, never a generic adb default.
serial=${SHADER_ANDROID_SERIAL:-ZE2223BQZT}
if [ "$serial" != ZE2223BQZT ]; then echo 'Shader test is scoped to USB Moto ZE2223BQZT' >&2; exit 1; fi
adb -s "$serial" get-state
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
"$ziran" build --project --module-path "$repo/tests" --root "$repo/tests" --target=c --define POSIX_THREADS --define ANDROID_BUILD --define __arm__ --no-main -o "$out" "$repo/tests/shader_android_test.zi"
"$toolchain/bin/armv7a-linux-androideabi26-clang" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$ziran_root/include" -iquote "$out" "$out"/*.c -landroid -lmediandk -llog -ldl -lm -lz -o "$out/shader-test"
remote=/data/local/tmp/kryon-shader-test
fixtures=/data/local/tmp/kryon-shader-fixtures
python3 "$repo/tests/image_pixels_fixtures.py"
trap 'adb -s "$serial" shell rm -f "$remote"; adb -s "$serial" shell rm -rf "$fixtures"' EXIT HUP INT TERM
adb -s "$serial" push "$out/shader-test" "$remote"
adb -s "$serial" shell chmod 700 "$remote"
adb -s "$serial" shell mkdir -p "$fixtures/build/scratch/image-pixels"
adb -s "$serial" push "$repo/build/scratch/image-pixels/rgba.png" "$repo/build/scratch/image-pixels/invalid.png" "$fixtures/build/scratch/image-pixels/"
adb -s "$serial" shell "cd $fixtures && $remote"
printf 'Physical Moto shader Pixmap/ANativeWindow: clip, alpha, bindings and window recreation passed\n'
