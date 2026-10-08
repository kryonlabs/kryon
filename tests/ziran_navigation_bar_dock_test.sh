#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_navigation_bar_dock_test.zi

set -- \
    --bind raster:RasterLine=ziran_navigation_bar_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_navigation_bar_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_navigation_bar_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_navigation_bar_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_navigation_bar_widget_host:RasterRoundedRectangleOutline \
    --bind paint_queue:RasterImage=ziran_navigation_bar_widget_host:RasterImage \
    --bind image_raster:ImageWidth=ziran_navigation_bar_dock_test:DockImageWidth \
    --bind image_raster:ImageHeight=ziran_navigation_bar_dock_test:DockImageHeight \
    --bind font_metrics:MeasureGlyphWidth=ziran_navigation_bar_dock_test:DockMeasureWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_navigation_bar_dock_test:DockMeasureHeight

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_navigation_bar_dock_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_navigation_bar_dock_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_navigation_bar_dock_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_navigation_bar_dock_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --root "$module_root" \
            --module-path "$module_dir" --module-path "$ziran_root/std" \
            -o "$output" "$module"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
                -I"$ziran_root/include" -I"$output" "$output"/*.c \
                -Wl,--gc-sections -lm -o "$output/test"
        else
            "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
                -I"$ziran_root/include" -I"$output" "$output"/*.cpp \
                -Wl,--gc-sections -lm -o "$output/test"
        fi
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/test"
    done
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_navigation_bar_dock_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$ziran_root/std" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
