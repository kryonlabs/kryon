#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
# The generated codec must match Kryon's widget records.
python3 "$repo/scripts/generate-widget-codecs.py" --check
python3 "$repo/scripts/generate-widget-codecs.py" --complete \
    --uses "$repo/src/backend/widget_codec.zi" --import '"widget_codec"' \
    --output "$work/widget_test_codecs.zi"
"$ziran" build --target=c --no-main --entry ziran_widget_host_test:main --root "$repo/tests" \
    $kryon_project_args --module-path "$work"  -o "$work/c" "$repo/tests/ziran_widget_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" "$work"/c/*.c \
    -Wl,--gc-sections -lm -o "$work/run"
"$work/run"
echo 'Widget host draws bundle widgets in their frame and refuses foreign calls'
