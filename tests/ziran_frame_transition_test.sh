#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_frame_transition_test.zi

set -- \
    --bind frame_transition:DisposeFrameTexture=ziran_frame_transition_host:DisposeFrameTexture \
    --bind font_metrics:MeasureGlyphWidth=ziran_frame_transition_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_frame_transition_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_frame_transition_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_frame_transition_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_frame_transition_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_frame_transition_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_frame_transition_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_frame_transition_host:RasterImage \
    --bind image_raster:ImageWidth=ziran_frame_transition_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_frame_transition_host:ImageHeight

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" "$@" \
    --entry ziran_frame_transition_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_frame_transition_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_frame_transition_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_frame_transition_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_frame_transition_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$repo/../ziran/std" -o "$output" "$module"
    mv "$output/ziran_frame_transition_test.go" "$output/frame_case.go"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
