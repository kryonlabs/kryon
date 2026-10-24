#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
tool_dir=$(dirname "$ziran")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
fill=raster_shape:RasterRoundedRectangle=reorder_widget_host:RasterRoundedRectangle
outline=raster_shape:RasterRoundedRectangleOutline=reorder_widget_host:RasterRoundedRectangleOutline

"$tool_dir/zi2zir" --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/ir" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zx" bundle --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/source.zx" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zx" bundle --root "$work/ir" \
    --module-path "$work/ir" \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/saved.zx" \
    "$work/ir/reorder_widget_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$tool_dir/ziran" run "$work/source.zx")" = 0
test "$("$tool_dir/ziran" run "$work/saved.zx")" = 0

"$tool_dir/ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" \
    "$repo/tests/reorder_widget_behavior.zi"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
