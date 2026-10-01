#!/bin/sh
# The tray runs only on private Xvfb displays; DISPLAY and the session bus are
# scrubbed so the test cannot reach the developer's desktop or panel.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/tests/toolchain.sh"
compiler=${ZI2C_BIN:-"$ziran_root/build/bin/zi2c"}
standard=${ZIRAN_STD:-"$ziran_root/std"}
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/tray.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$compiler" --entry tray_behavior:main --root "$root/tests" \
    --module-path "$root/src/backend" --module-path "$standard" \
    -o "$work/c" "$root/tests/tray_behavior.zi"
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -O2 -pthread -I"$include" \
    -iquote "$work/c" "$work/c"/*.c -ldl -o "$work/test"
private() {
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY \
        -u DBUS_SESSION_BUS_ADDRESS -u KRYON_TRAY_TEST_PRESENT "$@"
}
private "$work/test"
private KRYON_TRAY=status-icon KRYON_TRAY_TEST_PRESENT=1 \
    timeout 30s xvfb-run -a "$work/test"
if command -v dbus-run-session >/dev/null 2>&1; then
    private KRYON_TRAY_TEST_PRESENT=1 timeout 30s \
        dbus-run-session -- xvfb-run -a "$work/test"
fi
echo 'Tray menus, lifecycle, and hidden-host visibility: passed'
