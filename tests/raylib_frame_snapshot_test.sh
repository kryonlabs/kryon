#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export YUE_DESKTOP_RECOVERY=0
# Test-owned, ignored output; never replace shared compilers or host binaries.
work="$repo/work/frame-input"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
set -- --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std"
# Exercise the real WidgetHostCall and VM array transport with real raylib data.
guest="$repo/tests/frame_snapshot_guest.zi"
"$ziran" ir "$@" --define KRYON_HOSTED_UI -o "$work/guest-ir" "$guest"
"$ziran" bundle "$@" --define KRYON_HOSTED_UI --entry frame_snapshot_guest:main -o "$work/frame-snapshot.zx" "$guest"
"$ziran" bundle "$@" --define KRYON_HOSTED_UI --entry frame_snapshot_guest:main -o "$work/guest-saved.zx" "$work/guest-ir/frame_snapshot_guest.zir"
cmp "$work/frame-snapshot.zx" "$work/guest-saved.zx"
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
test -f "$archive" || { echo 'Set RAYLIB_A to the existing desktop SDL raylib archive' >&2; exit 1; }
"$ziran" build --target=c "$@" --entry raylib_frame_snapshot_test:main \
    -o "$work/host-c" "$repo/tests/raylib_frame_snapshot_test.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$work/host-c" \
    "$work/host-c"/*.c "$archive" "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
    -Wl,--gc-sections $(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm -o "$work/host-c/test"
(cd "$work" && timeout --kill-after=2s 45s xvfb-run -a -n 370 -s '-screen 0 640x480x24' ./host-c/test)
printf '%s\n' 'Frame snapshots: real raylib input and source/saved hosted VM boundary passed'
