#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
fill=kryon/raster_shape:RasterRoundedRectangle=reorder_widget_host:RasterRoundedRectangle
outline=kryon/raster_shape:RasterRoundedRectangleOutline=reorder_widget_host:RasterRoundedRectangleOutline

"$ziran" ir --root "$repo/tests" \
    $kryon_project_args  -o "$work/ir" \
    "$repo/tests/reorder_widget_behavior.zi"
"$ziran" bundle --root "$repo/tests" \
    $kryon_project_args  \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/source.zx" \
    "$repo/tests/reorder_widget_behavior.zi"
"$ziran" bundle --root "$work/ir" \
    --project --module-path "$work/ir" \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/saved.zx" \
    "$work/ir/reorder_widget_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    $kryon_project_args  \
    -o "$work/c" \
    "$repo/tests/reorder_widget_behavior.zi"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
