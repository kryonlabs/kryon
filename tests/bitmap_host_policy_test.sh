#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/bitmap-host-policy"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
"$ziran" build --project --root "$repo/tests" --target=c --no-main --entry bitmap_host_policy_test:main -o "$out" "$repo/tests/bitmap_host_policy_test.zi"
"${CC:-cc}" -O1 -std=c11 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$out" "$out"/*.c -Wl,--gc-sections -lm -o "$out/test"
"$out/test"
