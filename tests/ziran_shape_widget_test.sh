#!/bin/sh
# Line, Circle, Ring and Triangle: their nodes, the triangles they ask the
# host to fill, clipping, layout and retained frames, and Box's vertical
# fade, in portable bundles and on the C, C++ and Go targets.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
std="${include%/include}/std"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_shape_widget_test.zi
host=ziran_shape_widget_host

set -- \
    --bind raster_triangles:RasterTriangles=$host:RasterTriangles \
    --bind raster_shape:RasterRoundedRectangle=$host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=$host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=$host:RasterLine \
    --bind raster_text:RasterText=$host:RasterText \
    --bind raster_text:RasterTextClipped=$host:RasterTextClipped \
    --bind paint_queue:RasterImage=$host:RasterImage \
    --bind paint_queue:RasterImageMesh=$host:RasterImageMesh

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" "$@" \
    --entry ziran_shape_widget_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_shape_widget_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_shape_widget_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib" | tail -1)" = 0
test "$("$ziran" run "$work/saved.zib" | tail -1)" = 0

for target in c cpp; do
    output=$work/$target
    executable=--exe
    if test "$target" = cpp; then executable=; fi
    "$ziran" build --target="$target" --entry ziran_shape_widget_test:main \
        $executable --root "$repo/tests" --module-path "$repo/src/ui" \
        --module-path "$std" -o "$output" "$source"
    if test "$target" = cpp; then
        "${CXX:-c++}" -std=c++17 -I"$include" "$output"/*.cpp \
            -o "$output/ziran_shape_widget_test"
    fi
    test "$("$output/ziran_shape_widget_test" | tail -1)" = 0
done

output=$work/go
"$ziran" build --target=go --pkg main --exe \
    --entry ziran_shape_widget_test:main "$@" --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$std" -o "$output" "$source"
test "$(GOCACHE="$work/go-cache" GO111MODULE=off go run "$output"/*.go | tail -1)" = 0
printf 'Shape widgets passed in bundles and on C, C++ and Go.\n'
