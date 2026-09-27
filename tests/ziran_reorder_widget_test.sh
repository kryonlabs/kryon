#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tool_dir=$(dirname "${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
fill=raster_shape:RasterRoundedRectangle=reorder_widget_host:RasterRoundedRectangle
outline=raster_shape:RasterRoundedRectangleOutline=reorder_widget_host:RasterRoundedRectangleOutline

"$tool_dir/zi2zir" --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$work/ir" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zib" bundle --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/source.zib" \
    "$repo/tests/reorder_widget_behavior.zi"
"$tool_dir/zi2zib" bundle --root "$work/ir" \
    --module-path "$work/ir" \
    --bind "$fill" --bind "$outline" \
    --entry reorder_widget_behavior:main \
    -o "$work/saved.zib" \
    "$work/ir/reorder_widget_behavior.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$tool_dir/ziran" run "$work/source.zib")" = 0
test "$("$tool_dir/ziran" run "$work/saved.zib")" = 0

"$tool_dir/ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/c" \
    "$repo/tests/reorder_widget_behavior.zi"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"

"$tool_dir/ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/cpp" \
    "$repo/tests/reorder_widget_behavior.zi"
"${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$repo/../ziran/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp \
    -o "$work/test-cpp"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test-cpp"
