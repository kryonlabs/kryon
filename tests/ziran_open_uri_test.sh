#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" build --target=c --no-main --entry ziran_open_uri_test:main --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_open_uri_test.zi"
"${CC:-cc}" -O2 -std=c11 -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c \
    "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -lm -lpthread -o "$work/run"
(cd "$work" && ./run)
# Every platform's opener compiles: Windows, the browser, Plan 9, macOS and Android.
for define in _WIN32 PLATFORM_WEB KRYON_BACKEND_LIBDRAW __APPLE__ ANDROID_BUILD; do
    "$ziran_root/build/bin/zi2c" --no-main --define "$define" --root "$repo/src/backend" \
        --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
        -o "$work/$define" "$repo/src/backend/open_uri.zi"
done
echo 'OpenUri passes URIs unchanged, refuses empty, NUL and oversized ones, and compiles for every platform'
