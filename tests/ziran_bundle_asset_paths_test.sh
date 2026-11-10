#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/bundle-asset-path-test"
rm -rf "$work/c"
mkdir -p "$work/c"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" build --project --target=c --root "$repo/tests" --entry bundle_asset_paths:main \
    -o "$work/c" "$repo/tests/bundle_asset_paths.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ziran_root/include" -I"$work/c" "$work/c"/*.c -lm -o "$work/test"
"$work/test"
