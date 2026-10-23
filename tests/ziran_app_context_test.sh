#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
for variant in good future identify-write; do
    define=
    if [ "$variant" = future ]; then define='--define APP_BAD_API'; fi
    if [ "$variant" = identify-write ]; then define='--define APP_IDENTIFY_WRITE'; fi
    "$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" \
        --module-path "$repo/src/ui" --module-path "$ziran_root/std" --define KRYON_HOSTED_UI $define \
        --entry app_context_guest:main -o "$work/$variant.zib" "$repo/tests/app_context_guest.zi"
done
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/app_context_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/c" -I"$ziran_root/include" \
    "$work"/c/*.c "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections -lm -lpthread -o "$work/run"
# glibc overwrites freed memory here, so reading a reply the bundle has
# already reclaimed fails instead of passing by luck.
(cd "$work" && GLIBC_TUNABLES=glibc.malloc.tcache_count=0 MALLOC_PERTURB_=165 ./run)
echo 'Portable app identity, API compatibility, optional services, theme colours, replies, restart, quiet frame replay and identification isolation passed'
