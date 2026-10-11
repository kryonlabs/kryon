#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
raylib_source=${RAYLIB_SOURCE:-$("$ziran" pkg path raylib)}
raylib=${RAYLIB_A:-"$raylib_source/src/libraylib.a"}
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran_root/build/bin/zi2c" --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/raylib_trace_behavior.zi"
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ziran_root/include" -iquote "$work/c" "$work/c"/*.c "$raylib" $raylib_libs -o "$work/run"
(cd "$work" && ./run 2> console.txt)
grep -qx 'WARNING: IMAGE: retained' "$work/console.txt"
grep -qx 'INFO: values 42 3.25 kryon.test' "$work/console.txt"
grep -qx 'WARNING: 100% %s done' "$work/console.txt"
if grep -q 'IMAGE: hidden' "$work/console.txt"; then
    echo 'raylib trace kept a routine image note' >&2
    exit 1
fi
grep -qx '\[INFO\] file note' "$work/raylib_trace.log"
grep -qx '\[ERROR\] WGL test failure' "$work/raylib_trace.log"
# The va_list record of AArch64, Android's logcat and Windows compile too.
for defines in "--define ANDROID_BUILD --define __aarch64__" "--define ANDROID_BUILD --define __arm__" \
    "--define _WIN32" "--define KRYON_BACKEND_TERMI"; do
    # shellcheck disable=SC2086
    "$ziran_root/build/bin/zi2c" --no-main $defines --root "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$work/emit" "$repo/src/backend/raylib_trace.zi"
done
echo 'raylib trace: console filter, va_list formatting, literal text and log file passed'
