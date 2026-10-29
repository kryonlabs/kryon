#!/bin/sh
# A private Xvfb runs a client, finds the window it owns, captures, resizes,
# saves and reloads its pixels, and stops only what it started. The inherited
# DISPLAY is bogus: the test must never use it.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/private_display_test.zi"
# shellcheck disable=SC2046
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" $(find "$work/c" -type d -printf ' -iquote %p') \
    $(find "$work/c" -name '*.c') -lX11 -lcairo -lm -o "$work/private_display_test"
status=0
DISPLAY=:9999 "$work/private_display_test" "$work" || status=$?
if [ "$status" -ne 0 ]; then
    echo "private display test failed with $status" >&2
    cat "$work/xvfb.log" "$work/client.log" >&2 2>/dev/null || true
    exit 1
fi
echo 'Private display: own Xvfb, owned window by PID, title, capture, resize, PNG round trip and cleanup'
