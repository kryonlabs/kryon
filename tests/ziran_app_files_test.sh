#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'chmod -R u+rwx "$work"; rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/data" "$work/scope/readable/existing" "$work/scope/denied/existing/blocked"
printf 'preserve\n' > "$work/outside"
ln -s "$work/outside" "$work/data/escape"
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/app_files_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" -I"$ziran_root/include" \
    "$work"/c/*.c -Wl,--gc-sections -Wl,--wrap=fsync -lm -o "$work/run"
chmod 311 "$work/scope/denied" "$work/scope/denied/existing/blocked"
"$work/run" "$work/data" "$work/scope"
test "$(cat "$work/outside")" = preserve
test "$(stat -c %a "$work/data/empty.json")" = 600
test "$(stat -c %a "$work/data/documents")" = 700
echo 'Portable document storage: large documents, empty/missing distinction, isolation, locks, atomic replacement and private-root sync scope passed'
