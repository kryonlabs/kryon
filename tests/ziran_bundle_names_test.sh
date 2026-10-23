#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" build --target=c --root "$repo/tests" --module-path "$repo/src/backend" \
    -o "$work/c" "$repo/tests/ziran_bundle_names_test.zi"
"${CC:-cc}" -O2 -std=c99 -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c -o "$work/check"
"$work/check"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    -o "$work/ir" "$repo/tests/ziran_bundle_names_test.zi"
"$ziran" bundle --root "$work/ir" --entry ziran_bundle_names_test:main \
    -o "$work/names.zx" "$work/ir/ziran_bundle_names_test.zir"
test "$("$ziran" run "$work/names.zx")" = 0
echo 'Bundle module names: plain, package and named imports remain distinct'
