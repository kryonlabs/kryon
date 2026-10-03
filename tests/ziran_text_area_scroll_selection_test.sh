#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_area_scroll_selection_test.zi
set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_composition_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_composition_widget_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_composition_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_composition_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_composition_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_composition_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_composition_widget_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_composition_widget_host:RasterImage \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" --entry ziran_text_area_scroll_selection_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_text_area_scroll_selection_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_text_area_scroll_selection_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
for bundle in source saved; do
    result=$("$ziran" run "$work/$bundle.zib")
    printf '%s bundle: %s\n' "$bundle" "$result"
    test "$result" = 0
done
for target in c cpp go; do
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --exe \
            --entry ziran_text_area_scroll_selection_test:main "$@" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
    else
        "$ziran" build --target="$target" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$work/$target" \
                "$work/$target"/*.c -o "$work/$target/ziran_text_area_scroll_selection_test"
        else
            "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$work/$target" \
                "$work/$target"/*.cpp -o "$work/$target/ziran_text_area_scroll_selection_test"
        fi
    fi
    if test "$target" = go; then
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            GO111MODULE=off go run "$work/$target"/*.go
    else
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            "$work/$target/ziran_text_area_scroll_selection_test"
    fi
    printf '%s drag/scroll selection: passed\n' "$target"
done
