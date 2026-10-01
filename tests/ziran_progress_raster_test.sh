#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_progress_raster_test.zi

set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_progress_raster_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_progress_raster_host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_progress_raster_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_progress_raster_host:RasterTextClipped \
    --bind raster:RasterLine=ziran_progress_raster_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_progress_raster_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_progress_raster_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_progress_raster_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_progress_raster_test:main --root "$module_root" \
        --module-path "$module_dir" --module-path "$ziran_root/std" \
        "$@" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
