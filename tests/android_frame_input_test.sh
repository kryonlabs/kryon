#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY KRYON_TEST_DRIVER
export YUE_DESKTOP_RECOVERY=0
# Test-owned, ignored output; never replace shared compilers or host binaries.
work="$repo/work/android-frame-input"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
test -f "$archive" || { echo 'Set RAYLIB_A to the existing desktop SDL raylib archive' >&2; exit 1; }
# The canonical Android host's frame, driven by desktop raylib on private Xvfb.
"$ziran" build --target=c --root "$repo/tests" --module-path "$repo/tests/android_frame_input" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    --no-main -o "$work/c" "$repo/tests/android_frame_input_test.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c "$archive" "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
    -Wl,--gc-sections -Wl,--wrap=EndDrawing -Wl,--wrap=__android_log_write \
    $(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm -o "$work/c/test"
(cd "$work" && timeout --kill-after=2s 45s xvfb-run -a -n 371 -s '-screen 0 640x480x24' ./c/test)
printf '%s\n' 'Android frame input: canonical host publication and revocation passed'
