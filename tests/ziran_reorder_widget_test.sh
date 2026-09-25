#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tool_dir=$(dirname "${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$tool_dir/zi2zir" --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/ir" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zib" bundle --root "$repo/tests" \
    --module-path "$repo/src/ui" \
    --entry reorder_widget_behavior:Answer \
    -o "$work/source.zib" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zib" bundle --root "$work/ir" \
    --module-path "$work/ir" \
    --entry reorder_widget_behavior:Answer \
    -o "$work/saved.zib" \
    "$work/ir/reorder_widget_behavior.zir"
cmp "$work/source.zib" "$work/saved.zib"

"$tool_dir/zi2c" --no-main --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/c" \
    "$repo/tests/reorder_widget_behavior.zi"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$repo/../ziran/include" -I"$work/c" \
    "$repo/tests/ziran_reorder_widget_test.c" "$work/c"/*.c \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
