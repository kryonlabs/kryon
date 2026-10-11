#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
# The generated codec must match Kryon's widget records.
python3 "$repo/scripts/generate-widget-codecs.py" --check
"$ziran" build --target=c --no-main --entry ziran_widget_host_test:main --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/ziran_widget_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" "$work"/c/*.c \
    -Wl,--gc-sections -lm -o "$work/run"
"$work/run"
echo 'Widget host draws bundle widgets in their frame and refuses foreign calls'
