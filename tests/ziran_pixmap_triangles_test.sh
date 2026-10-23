#!/bin/sh
# Solid triangles through the pixmap host: the raster effect and the shape
# widgets in retained frames, on the native targets and in portable bundles.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch/pixmap-triangles"
work=$(mktemp -d "$repo/build/scratch/pixmap-triangles/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_triangles_test.zi"
set -- --bind raster_triangles:RasterTriangles=pixmap:RasterTriangles \
    --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind paint_queue:RasterImage=pixmap:RasterImage \
    --bind paint_queue:RasterImageMesh=pixmap:RasterImageMesh
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in ${PIXMAP_TRIANGLE_TARGETS:-c cpp go}; do
    executable=--exe; if test "$target" = cpp; then executable=; fi
    if test "$target" = go; then
        "$ziran" build --project --root "$repo/tests" --target=go \
            --entry pixmap_triangles_test:main "$@" --pkg main --exe \
            -o "$work/go" "$source"
    else
        "$ziran" build --project --root "$repo/tests" --target="$target" \
            --entry pixmap_triangles_test:main $executable \
            -o "$work/$target" "$source"
    fi
    if test "$target" = cpp; then
        "${CXX:-c++}" -std=c++17 -O2 -I"$ziran_root/include" "$work/$target"/*.cpp \
            -lm -o "$work/$target/pixmap_triangles_test"
    fi
    if test "$target" = go; then
        test "$(GO111MODULE=off go run "$work/$target"/*.go | tail -1)" = 0
    else
        test "$("$work/$target/pixmap_triangles_test" | tail -1)" = 0
    fi
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_triangles_test:main \
    "$@" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry pixmap_triangles_test:main "$@" -o "$work/saved.zx" \
    "$work/ir/pixmap_triangles_test.zir"
test "$("$ziran" run "$work/source.zx" | tail -1)" = 0
test "$("$ziran" run "$work/saved.zx" | tail -1)" = 0
printf 'Pixmap triangles match on every target and in bundles.\n'
