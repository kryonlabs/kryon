#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/style-cache/cost"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" build --target=c --no-main --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work" "$repo/tests/style_cache_cost.zi"
"${CC:-cc}" -O2 -shared -fPIC -I"$ziran_root/include" -I"$work" \
    "$work"/*.c -Wl,-Bsymbolic -o "$work/libstyle-cost.so"
KRYON_STYLE_COST_LIBRARY="$work/libstyle-cost.so" \
    python3 "$repo/tests/style_cache_cost.py"
