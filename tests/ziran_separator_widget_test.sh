#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

portable=$repo/tests/separator_widget_portable_test.zi
set -- \
    --bind raster:RasterLine=separator_widget_host:RasterLine \
    --bind raster_text:RasterText=separator_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=separator_widget_host:RasterTextClipped \
    --bind font_metrics:MeasureGlyphWidth=separator_widget_host:MeasureGlyphWidth \
    --bind raster_shape:RasterRoundedRectangle=separator_widget_host:RasterRoundedRectangle

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry separator_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry separator_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/separator_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

native=$repo/tests/separator_widget_native.zi

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "separator_widget_native.hpp"
#include <cassert>
#include <cstring>
#define HOST extern "C"
#else
#include "separator_widget_native.h"
#include <assert.h>
#include <string.h>
#define HOST
#endif
static int lines, texts, measures, bullets;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)typeface;
    assert(value.length == 1 && value.data[0] == 'A' && font == 14);
    measures++;
    return 10;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface;
    assert(0 && "Separator should not measure line height");
    return 0;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(color.r == 0x11 && color.g == 0x22 &&
           color.b == 0x33 && color.a == 0x44);
    if(lines == 0)
        assert(line.x == 20 && line.y == 20 &&
               line.width == 0 && line.height == 40);
    else
        assert(line.x == 58 && line.y == 30 &&
               line.width == 82 && line.height == 0);
    lines++;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    assert(value.length == 1 && value.data[0] == 'A');
    assert(x == 40 && y == 23 && font == 14);
    assert(color.r == 0xaa && color.g == 0xbb &&
           color.b == 0xcc && color.a == 0xdd);
    texts++;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    assert(0 && "Separator should not draw clipped text");
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 156 && bounds.y == 26 &&
           bounds.width == 8 && bounds.height == 8);
    assert(radius == 0.5f && segments == 32);
    assert(color.r == 0x12 && color.g == 0x34 &&
           color.b == 0x56 && color.a == 0xff);
    bullets++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
    assert(0 && "Separator should not draw rounded outlines");
}
HOST void RasterImage(String path, uint32_t texture_id,
    Rectangle source, Rectangle destination, Rectangle clip,
    Vector2 origin, float rotation, float radius, Color tint) {
    (void)path; (void)texture_id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0 && "Separator should not draw images");
}
int main(void) {
    assert(Answer() == 42 && lines == 2 && texts == 1 &&
           measures == 1 && bullets == 1);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$output" "$native"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/separator_widget_test.go" <<'GO'
package ziran
import "testing"
type separatorHost struct { t *testing.T; lines, texts, measures, bullets int }
func (h *separatorHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "A" || font != 14 { h.t.Fatal("width") }
    h.measures++
    return 10
}
func (h *separatorHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { h.t.Fatal("line height"); return 0 }
func (h *separatorHost) RasterLine(line Rectangle, color Color) {
    if color.R != 0x11 || color.G != 0x22 ||
       color.B != 0x33 || color.A != 0x44 { h.t.Fatal("color") }
    if h.lines == 0 {
        if line.X != 20 || line.Y != 20 ||
           line.Width != 0 || line.Height != 40 { h.t.Fatal("vertical") }
    } else if line.X != 58 || line.Y != 30 ||
       line.Width != 82 || line.Height != 0 { h.t.Fatal("label line") }
    h.lines++
}
func (h *separatorHost) RasterText(value string, x, y, font int32,
    color Color) {
    if value != "A" || x != 40 || y != 23 || font != 14 ||
       color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 0xdd { h.t.Fatal("label") }
    h.texts++
}
func (h *separatorHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    h.t.Fatal("Separator should not draw clipped text")
}
func (h *separatorHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 156 || bounds.Y != 26 ||
       bounds.Width != 8 || bounds.Height != 8 ||
       radius != 0.5 || segments != 32 ||
       color.R != 0x12 || color.G != 0x34 ||
       color.B != 0x56 || color.A != 0xff { h.t.Fatal("bullet") }
    h.bullets++
}
func (h *separatorHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("Separator should not draw rounded outlines")
}
func (h *separatorHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    h.t.Fatal("Separator should not draw images")
}
func TestSeparatorWidget(t *testing.T) {
    host := &separatorHost{t: t}
    SetFontMetricsHost(host)
    SetRasterHost(host)
    SetRasterTextHost(host)
    SetRasterShapeHost(host)
    SetPaintQueueHost(host)
    if SeparatorWidgetNative_Answer() != 42 || host.lines != 2 ||
       host.texts != 1 || host.measures != 1 || host.bullets != 1 {
        t.Fatal("separator composition")
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
