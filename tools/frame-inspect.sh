#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
build=$repo/build/frame-inspect
mkdir -p "$build"
exec 9>"$build/lock"
flock 9
"$ziran" build --target=c --root "$repo/tools" --module-path "$ziran_root/std" \
    --entry frame_inspect:main -o "$build/c" "$repo/tools/frame_inspect.zi"
"${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -I"$build/c" \
    "$build/c"/*.c -o "$build/run"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    YUE_DESKTOP_RECOVERY=0 "$build/run" "$@"
