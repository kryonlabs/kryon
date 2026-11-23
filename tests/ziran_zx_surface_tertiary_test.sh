#!/bin/sh
# The portable Surface reports the middle button through a tertiary pointer
# sample, beside the primary and secondary ones.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER GDK_DISPLAY

"$ziran" build --project --target=c --root "$repo/tests" --bind-host pixmap \
    --entry zx_surface_tertiary_test:main --exe \
    -o "$work/c" "$repo/tests/zx_surface_tertiary_test.zi"
"$work/c/zx_surface_tertiary_test"

"$ziran" bundle --project --root "$repo/tests" --bind-host pixmap \
    --entry zx_surface_tertiary_test:main \
    -o "$work/test.zx" "$repo/tests/zx_surface_tertiary_test.zi"
test "$("$ziran" run "$work/test.zx" | tail -1)" = 0
echo "Surface tertiary pointer: button 2 press, hold and release; buttons 1 and 3 unchanged, C and saved bundle"
