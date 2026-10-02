#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/menu-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_menu_widget_test.zi
entry=ziran_menu_widget_test:main
set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_context_menu_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_context_menu_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_context_menu_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_context_menu_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_context_menu_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_context_menu_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_context_menu_widget_host:RasterTextClipped
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --entry "$entry" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    module_root=$repo/tests
    if test "$form" = saved; then input=$work/ir/ziran_menu_widget_test.zir; module_root=$work/ir; fi
    output=$work/$form-c
    "$ziran" build --target=c --exe --entry "$entry" \
        --root "$module_root" --module-path "$repo/src/ui" -o "$output" "$input"
    timeout --kill-after=2s 20s "$output/ziran_menu_widget_test"
    output=$work/$form-cpp
    "$ziran" build --target=cpp --entry "$entry" \
        --root "$module_root" --module-path "$repo/src/ui" -o "$output" "$input"
    "${CXX:-c++}" -std=c++17 -O1 -I"$output" "$output"/*.cpp -o "$output/run"
    timeout --kill-after=2s 20s "$output/run"
    "$ziran" build --target=go --pkg main --exe --entry "$entry" "$@" \
        --root "$module_root" --module-path "$repo/src/ui" -o "$work/$form-go" "$input"
    timeout --kill-after=2s 45s env GO111MODULE=off go run "$work/$form-go"/*.go
    "$ziran" bundle --entry "$entry" "$@" --root "$module_root" --module-path "$repo/src/ui" \
        -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 0
done
echo 'Menu source and saved IR passed C/C++/Go and portable behavior checks'
