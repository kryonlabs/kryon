#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/data"
printf 'preserve\n' > "$work/outside"
ln -s "$work/outside" "$work/data/escape"
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/app_files_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" -I"$ziran_root/include" \
    "$work"/c/*.c -Wl,--gc-sections -lm -o "$work/run"
"$work/run" "$work/data"
test "$(cat "$work/outside")" = preserve
test "$(stat -c %a "$work/data/empty.json")" = 600
test "$(stat -c %a "$work/data/documents")" = 700
echo 'Portable document storage: large documents, empty/missing distinction, isolation, locks and atomic replacement passed'
