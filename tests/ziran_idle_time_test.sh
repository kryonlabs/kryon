#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
gio_define=
gio_libraries=
if pkg-config --exists gio-2.0; then
    gio_define="--define APP_HAVE_GIO"
    gio_libraries=$(pkg-config --libs gio-2.0)
fi
# The GIO branch is compiled when available; the test never reaches a real
# session bus or display.
# shellcheck disable=SC2086
"$ziran_root/build/bin/zi2c" --no-main --define PLATFORM_DESKTOP $gio_define --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_idle_time_test.zi"
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -O2 -I"$ziran_root/include" -iquote "$work/c" \
    "$work"/c/*.c $gio_libraries -o "$work/run"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY \
    DBUS_SESSION_BUS_ADDRESS=invalid: "$work/run"
echo 'Idle time: Wayland detection and an unknown idle time without a session bus passed'
