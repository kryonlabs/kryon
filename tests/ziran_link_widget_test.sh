#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_link_widget_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_link_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_link_widget_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_link_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_link_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_link_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_link_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_link_widget_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_link_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    "$@" --entry ziran_link_widget_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry ziran_link_widget_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_link_widget_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/c/app"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -lm -o "$work/cpp/app"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/go" "$source"
mv "$work/go/ziran_link_widget_test.go" "$work/go/link_case.go"
cat > "$work/go/main.go" <<'GO'
package main

type linkHost struct{}

func (linkHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    return ZiranLinkWidgetHost_MeasureGlyphWidth(value, font, typeface)
}

func (linkHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    return ZiranLinkWidgetHost_MeasureGlyphLineHeight(font, typeface)
}

func (linkHost) RasterText(value string, x, y, font int32, color Color) {
    ZiranLinkWidgetHost_RasterText(value, x, y, font, color)
}

func (linkHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    ZiranLinkWidgetHost_RasterTextClipped(value, x, y, font, color, clip)
}

func (linkHost) RasterRoundedRectangle(bounds Rectangle, radius float32,
    segments int32, color Color) {
    ZiranLinkWidgetHost_RasterRoundedRectangle(bounds, radius, segments, color)
}

func (linkHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    ZiranLinkWidgetHost_RasterRoundedRectangleOutline(bounds, radius,
        segments, width, color)
}

func (linkHost) RasterLine(line Rectangle, color Color) {
    ZiranLinkWidgetHost_RasterLine(line, color)
}

func (linkHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    ZiranLinkWidgetHost_RasterImage(path, textureID, source, destination,
        clip, origin, rotation, radius, tint)
}

func main() {
    host := linkHost{}
    SetFontMetricsHost(host)
    SetRasterTextHost(host)
    SetRasterShapeHost(host)
    SetRasterHost(host)
    SetPaintQueueHost(host)
    if ZiranLinkWidgetTest_Main() != 0 { panic("link behavior failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
