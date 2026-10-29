#!/bin/sh
# A private headless Chromium driven over the DevTools pipe, with the test's
# own static server, natively in C. Skipped without Chromium.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
program=${CHROMIUM:-$(command -v chromium || command -v google-chrome || true)}
if [ -z "$program" ]; then
    echo 'DevTools browser test skipped: no Chromium'
    exit 0
fi
# Chromium's socket paths must stay short.
work=$(mktemp -d "$repo/build/devtools.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/site"
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/devtools_browser_test.zi"
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$work/c" "$work/c"/*.c -lm -o "$work/run"
status=0
CHROMIUM=$program DEVTOOLS_TEST_DIR=$work "$work/run" || status=$?
if [ "$status" -ne 0 ]; then
    echo "DevTools browser test failed with $status" >&2
    tail -20 "$work/profile/browser.log" >&2 || true
    exit 1
fi
echo 'DevTools: served pages, evaluation, clicks, keys, interception, page exceptions and screenshots'
