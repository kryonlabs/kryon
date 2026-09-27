#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/selectable_widget_behavior.zi
portable=$repo/tests/selectable_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphLineHeight=selectable_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=selectable_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=selectable_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=selectable_widget_host:RasterLine \
    --bind raster_text:RasterText=selectable_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=selectable_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=selectable_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/selectable_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

for target in c cpp; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
        -o "$output" "$portable"
    if test "$target" = c; then
        "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c -o "$output/app"
    else
        "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
done

for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        module=$portable
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/selectable_widget_portable_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    "$ziran" build --target=go --pkg main --exe \
        --entry selectable_widget_portable_test:main --root "$module_root" \
        --module-path "$module_dir" --module-path "$repo/../ziran/std" \
        "$@" -o "$output" "$module"
    mv "$output/selectable_widget_portable_test.go" "$output/selectable_widget_case.go"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
