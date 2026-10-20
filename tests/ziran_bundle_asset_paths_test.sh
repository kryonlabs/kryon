#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/bundle-asset-path-test"
mkdir -p "$work/c"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" build --target=c --root "$repo/tests" --entry bundle_asset_paths:main \
    --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/bundle_asset_paths.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ziran_root/include" -I"$work/c" "$work/c"/*.c -lm -o "$work/test"
"$work/test"
