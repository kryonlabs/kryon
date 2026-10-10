#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/system_palette_buttons.zi"
# The palette here is given directly; no host theme is read.
cat > "$work/host.c" <<'C'
#include <stdint.h>
uint32_t SystemThemeColorHost(int32_t role) { (void)role; return 0; }
C
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c "$work/host.c" -lm -o "$work/palette-test"
"$work/palette-test"
