#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_mixed_tree_test.zi

set -- \
    --bind raster:RasterLine=ziran_mixed_tree_host:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=ziran_mixed_tree_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_mixed_tree_host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_mixed_tree_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_mixed_tree_host:RasterTextClipped \
    --bind font_metrics:MeasureGlyphWidth=ziran_mixed_tree_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_mixed_tree_host:MeasureGlyphLineHeight \
    --bind paint_queue:RasterImage=ziran_mixed_tree_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    "$@" --entry ziran_mixed_tree_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry ziran_mixed_tree_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_mixed_tree_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/go" "$source"
mv "$work/go/ziran_mixed_tree_test.go" "$work/go/mixed_case.go"
cat > "$work/go/main.go" <<'GO'
package main

type mixedHost struct{}

func (mixedHost) RasterLine(bounds Rectangle, color Color) {
    ZiranMixedTreeHost_RasterLine(bounds, color)
}

func (mixedHost) RasterRoundedRectangle(bounds Rectangle, radius float32,
    segments int32, color Color) {
    ZiranMixedTreeHost_RasterRoundedRectangle(bounds, radius, segments, color)
}

func (mixedHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    ZiranMixedTreeHost_RasterRoundedRectangleOutline(bounds, radius,
        segments, width, color)
}

func (mixedHost) RasterText(value string, x, y, font int32, color Color) {
    ZiranMixedTreeHost_RasterText(value, x, y, font, color)
}

func (mixedHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    ZiranMixedTreeHost_RasterTextClipped(value, x, y, font, color, clip)
}

func (mixedHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 {
    return ZiranMixedTreeHost_MeasureGlyphWidth(value, font, face)
}

func (mixedHost) MeasureGlyphLineHeight(font int32, face string) int32 {
    return ZiranMixedTreeHost_MeasureGlyphLineHeight(font, face)
}

func (mixedHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    ZiranMixedTreeHost_RasterImage(path, textureID, source, destination,
        clip, origin, rotation, radius, tint)
}

func main() {
    host := mixedHost{}
    SetRasterHost(host)
    SetRasterShapeHost(host)
    SetRasterTextHost(host)
    SetFontMetricsHost(host)
    SetPaintQueueHost(host)
    if ZiranMixedTreeTest_Main() != 0 { panic("mixed tree behavior failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
