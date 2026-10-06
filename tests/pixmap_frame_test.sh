#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch/pixmap-frame"
work=$(mktemp -d "$repo/build/scratch/pixmap-frame/run.XXXXXX")
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_frame_test.zi"
set -- --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind paint_queue:RasterImage=pixmap:RasterImage
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in c cpp go; do
    executable=--exe; if test "$target" = cpp; then executable=; fi
    package=; if test "$target" = go; then package='--pkg main'; fi
    if test "$target" = go; then
        "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_frame_test:main "$@" $package $executable -o "$work/$target" "$source"
    else
        "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_frame_test:main $executable -o "$work/$target" "$source"
    fi
    if test "$target" = cpp; then
        rm -f "$work/$target/entry.cpp"
        "${CXX:-c++}" -std=c++17 -O2 -I"$ziran_root/include" "$work/$target"/*.cpp -lm -o "$work/$target/pixmap_frame_test"
    fi
    if test "$target" = go; then GO111MODULE=off go run "$work/$target"/*.go
    else "$work/$target/pixmap_frame_test"; fi
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_frame_test:main "$@" -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_frame_test:main "$@" -o "$work/saved.zib" "$work/ir/pixmap_frame_test.zir"
test "$("$ziran" run "$work/source.zib" | tail -1)" = 0
test "$("$ziran" run "$work/saved.zib" | tail -1)" = 0
