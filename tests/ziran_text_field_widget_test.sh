#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_field_widget_test.zi

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
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_text_field_widget_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_text_field_widget_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_text_field_widget_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_text_field_widget_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_text_field_widget_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$ziran_root/std" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
