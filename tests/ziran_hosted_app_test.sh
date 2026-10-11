#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/notes"
printf 'hello\n' > "$work/notes/hello.txt"
"$ziran" bundle --root "$repo/tests" --module-path "$ziran_root/std" --asset-dir "notes=$work/notes" \
    --entry hosted_app_guest:main -o "$work/hosted_app_guest.zib" "$repo/tests/hosted_app_guest.zi"
"$ziran" bundle --root "$repo/tests" --module-path "$ziran_root/std" \
    --entry hosted_app_loop:main -o "$work/hosted_app_loop.zib" "$repo/tests/hosted_app_loop.zi"
"$ziran" build --target=c --no-main --entry ziran_hosted_app_test:main --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_hosted_app_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" -I"$ziran_root/include" \
    "$work"/c/*.c "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections -lm -lpthread -o "$work/run"
(cd "$work" && ./run)
echo 'Hosted app checks capabilities, keeps a working instance through a rejected restart and traps runaway runs'
