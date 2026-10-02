#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_paint_text_large_test.zi
entry=ziran_paint_text_large_test:main
set -- \
    --bind raster:RasterLine=ziran_paint_text_large_test:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=ziran_paint_text_large_test:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_paint_text_large_test:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_paint_text_large_test:RasterText \
    --bind raster_text:RasterTextClipped=ziran_paint_text_large_test:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_paint_text_large_test:RasterImage
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    module_root=$repo/tests
    module_path=$repo/src/ui
    if test "$form" = saved; then
        input=$work/ir/ziran_paint_text_large_test.zir
        module_root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" build --target=c --entry "$entry" --root "$module_root" \
        --module-path "$module_path" --module-path "$ziran_root/std" \
        --exe -o "$work/$form-c" "$input"
    timeout 15 "$work/$form-c/ziran_paint_text_large_test"
    "$ziran" build --target=cpp --entry "$entry" --root "$module_root" \
        --module-path "$module_path" --module-path "$ziran_root/std" \
        -o "$work/$form-cpp" "$input"
    "${CXX:-c++}" -std=c++17 -O1 -I"$work/$form-cpp" "$work/$form-cpp"/*.cpp \
        -o "$work/$form-cpp/run"
    timeout 15 "$work/$form-cpp/run"
    "$ziran" build --target=go --pkg main --entry "$entry" "$@" \
        --root "$module_root" --module-path "$module_path" \
        --module-path "$ziran_root/std" --exe -o "$work/$form-go" "$input"
    timeout 30 env GO111MODULE=off go run "$work/$form-go"/*.go
    "$ziran" bundle --entry "$entry" "$@" --root "$module_root" \
        --module-path "$module_path" --module-path "$ziran_root/std" \
        -o "$work/$form.zib" "$input"
    test "$(timeout 30 "$ziran" run "$work/$form.zib")" = 0
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Large Kryon paint labels retain their text across source, saved IR and native/portable targets'
