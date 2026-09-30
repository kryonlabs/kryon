#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
include=${ZIRAN_INCLUDE:-"$repo/../../ziranlang/ziran/include"}
std="${include%/include}/std"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_frame_retained_test.zi

set -- \
    --bind raster:RasterLine=ziran_frame_retained_host:RasterLine \
    --bind raster_text:RasterText=ziran_frame_retained_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_frame_retained_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_frame_retained_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" "$@" \
    --entry ziran_frame_retained_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_frame_retained_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_frame_retained_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_frame_retained_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_frame_retained_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$std" -o "$output" "$module"
    for file in "$output"/*_test.go; do
        [ -f "$file" ] || continue
        mv "$file" "${file%_test.go}_case.go"
    done
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
