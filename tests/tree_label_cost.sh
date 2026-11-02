#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/tree-labels/cost"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work" "$repo/tests/tree_label_cost.zi"
"${CC:-cc}" -O2 -shared -fPIC -I"$ziran_root/include" -I"$work" "$work"/*.c -Wl,-Bsymbolic -o "$work/libtree-label-cost.so"
TREE_LABEL_COST_LIBRARY="$work/libtree-label-cost.so" python3 "$repo/tests/tree_label_cost.py"
