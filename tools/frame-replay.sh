#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_lib=${ZIRAN_LIB:-"$ziran_root/build/libziran.a"}
if test "$#" -ne 4; then
    echo "usage: $0 app.zib pointer-module input.trace output.json" >&2
    exit 2
fi

build=$repo/build/frame-replay
mkdir -p "$build"
exec 9>"$build/lock"
flock 9
"$ziran" build --target=c --root "$repo/tools" \
    --module-path "$ziran_root/std" --entry frame_replay:main \
    -o "$build/c" "$repo/tools/frame_replay.zi"
"${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -I"$build/c" \
    "$build/c"/*.c "$ziran_lib" -o "$build/replay"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
    "$build/replay" "$@"
