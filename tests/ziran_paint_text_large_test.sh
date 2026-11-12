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
    --bind kryon/raster:RasterLine=ziran_paint_text_large_test:RasterLine \
    --bind kryon/raster_shape:RasterRoundedRectangle=ziran_paint_text_large_test:RasterRoundedRectangle \
    --bind kryon/raster_shape:RasterRoundedRectangleOutline=ziran_paint_text_large_test:RasterRoundedRectangleOutline \
    --bind kryon/raster_text:RasterText=ziran_paint_text_large_test:RasterText \
    --bind kryon/raster_text:RasterTextClipped=ziran_paint_text_large_test:RasterTextClipped \
    --bind kryon/raster_text:RasterTextRotated=ziran_mesh_noop_host:RasterTextRotated \
    --bind kryon/paint_queue:RasterImage=ziran_paint_text_large_test:RasterImage \
    --bind kryon/paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh
"$ziran" ir --root "$repo/tests" $kryon_project_args \
     -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    module_root=$repo/tests
    module_path="$kryon_project_args"
    if test "$form" = saved; then
        input=$work/ir/ziran_paint_text_large_test.zir
        module_root=$work/ir
        module_path="--project --module-path $work/ir"
    fi
    "$ziran" build --target=c --entry "$entry" --root "$module_root" \
        $module_path  \
        --exe -o "$work/$form-c" "$input"
    timeout 15 "$work/$form-c/ziran_paint_text_large_test"
    "$ziran" build --target=go --pkg main --entry "$entry" "$@" \
        --root "$module_root" $module_path \
         --exe -o "$work/$form-go" "$input"
    timeout 30 env GO111MODULE=off go run "$work/$form-go"/*.go
    "$ziran" bundle --entry "$entry" "$@" --root "$module_root" \
        $module_path  \
        -o "$work/$form.zx" "$input"
    test "$(timeout 30 "$ziran" run "$work/$form.zx")" = 0
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Kryon paint labels and image paths retain their bytes across source, saved IR and native/portable targets'
