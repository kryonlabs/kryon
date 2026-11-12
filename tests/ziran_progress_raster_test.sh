#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_progress_raster_test.zi

set -- \
    --bind kryon/raster_shape:RasterRoundedRectangle=ziran_progress_raster_host:RasterRoundedRectangle \
    --bind kryon/raster_shape:RasterRoundedRectangleOutline=ziran_progress_raster_host:RasterRoundedRectangleOutline \
    --bind kryon/raster_text:RasterText=ziran_progress_raster_host:RasterText \
    --bind kryon/raster_text:RasterTextClipped=ziran_progress_raster_host:RasterTextClipped \
    --bind kryon/raster_text:RasterTextRotated=ziran_mesh_noop_host:RasterTextRotated \
    --bind kryon/raster:RasterLine=ziran_progress_raster_host:RasterLine \
    --bind kryon/paint_queue:RasterImage=ziran_progress_raster_host:RasterImage \
    --bind kryon/paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh

"$ziran" ir --root "$repo/tests" $kryon_project_args  \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" $kryon_project_args  \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --project --module-path "$work/ir" \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/saved.zx" "$work/ir/ziran_progress_raster_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    $kryon_project_args  -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"


for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir="$kryon_project_args"
    else
        module=$work/ir/ziran_progress_raster_test.zir
        module_root=$work/ir
        module_dir="--project --module-path $work/ir"
    fi
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_progress_raster_test:main --root "$module_root" \
        $module_dir  \
        "$@" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
