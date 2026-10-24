#!/bin/sh
# Warped images through the pixmap host: the raster effect and the Image
# widget's queued grid, on every native target and in portable bundles.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch/pixmap-mesh"
work=$(mktemp -d "$repo/build/scratch/pixmap-mesh/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_mesh_test.zi"
set -- --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind paint_queue:RasterImage=pixmap:RasterImage \
    --bind paint_queue:RasterImageMesh=pixmap:RasterImageMesh \
    --bind image_raster:ImageWidth=pixmap:ImageWidth \
    --bind image_raster:ImageHeight=pixmap:ImageHeight \
    --bind font_metrics:MeasureGlyphWidth=pixmap:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=pixmap:MeasureGlyphLineHeight
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in c go; do
    executable=--exe
    package=; if test "$target" = go; then package='--pkg main'; fi
    if test "$target" = go; then
        "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_mesh_test:main "$@" $package $executable -o "$work/$target" "$source"
    else
        "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_mesh_test:main $executable -o "$work/$target" "$source"
    fi
    if test "$target" = go; then
        test "$(GO111MODULE=off go run "$work/$target"/*.go | tail -1)" = 0
    else
        test "$("$work/$target/pixmap_mesh_test" | tail -1)" = 0
    fi
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_mesh_test:main "$@" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_mesh_test:main "$@" -o "$work/saved.zx" "$work/ir/pixmap_mesh_test.zir"
test "$("$ziran" run "$work/source.zx" | tail -1)" = 0
test "$("$ziran" run "$work/saved.zx" | tail -1)" = 0
printf 'Warped images match on every target and in bundles.\n'
