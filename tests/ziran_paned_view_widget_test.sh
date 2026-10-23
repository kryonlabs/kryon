#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/paned_view_behavior.zi
portable=$repo/tests/paned_view_portable_test.zi
bind_fill=raster_shape:RasterRoundedRectangle=paned_view_host:RasterRoundedRectangle
bind_outline=raster_shape:RasterRoundedRectangleOutline=paned_view_host:RasterRoundedRectangleOutline
bind_line=raster:RasterLine=paned_view_host:RasterLine
bind_text=raster_text:RasterText=paned_view_host:RasterText
bind_clipped=raster_text:RasterTextClipped=paned_view_host:RasterTextClipped
bind_image=paint_queue:RasterImage=paned_view_host:RasterImage
bind_mesh=paint_queue:RasterImageMesh=paned_view_host:RasterImageMesh

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --bind "$bind_mesh" --entry paned_view_portable_test:main \
    -o "$work/source.zx" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --bind "$bind_mesh" --entry paned_view_portable_test:main \
    -o "$work/saved.zx" "$work/ir/paned_view_portable_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for target in c cpp; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
        -o "$output" "$portable"
    if test "$target" = c; then
        "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$ziran_root/include" -I"$output" \
            "$output"/*.c -o "$output/app"
    else
        "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$ziran_root/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
done

for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        module=$portable
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/paned_view_portable_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    "$ziran" build --target=go --pkg main --exe \
        --entry paned_view_portable_test:main --root "$module_root" \
        --module-path "$module_dir" --module-path "$ziran_root/std" \
        --bind "$bind_fill" --bind "$bind_outline" \
        --bind "$bind_line" --bind "$bind_text" \
        --bind "$bind_clipped" --bind "$bind_image" --bind "$bind_mesh" \
        -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
