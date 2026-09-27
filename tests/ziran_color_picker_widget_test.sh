#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/color_picker_widget_behavior.zi
portable=$repo/tests/color_picker_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphLineHeight=color_picker_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=color_picker_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=color_picker_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=color_picker_widget_host:RasterLine \
    --bind raster_text:RasterText=color_picker_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=color_picker_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=color_picker_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry color_picker_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry color_picker_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/color_picker_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "color_picker_widget_behavior.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "color_picker_widget_behavior.h"
#include <assert.h>
#define HOST
#endif
static int swatches, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface; return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 190 &&
           bounds.y + bounds.height <= 250);
    if (bounds.y == 214 && bounds.height == 36) {
        assert(color.r == 64 && color.g == 128 && color.b == 191);
        swatches++;
    }
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
}
HOST void RasterLine(Rectangle bounds, Color color) {
    (void)bounds; (void)color; assert(0);
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color; assert(0);
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)x; (void)y; (void)color;
    assert(value.length > 0 && font == 14);
    assert(clip.x >= 10 && clip.y >= 10 &&
           clip.x + clip.width <= 190 &&
           clip.y + clip.height <= 250);
    labels++;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
int main(void) {
    for (int phase = 0; phase < 5; phase++) {
        assert(Frame() == phase);
        assert(swatches == phase + 1 && labels == (phase + 1) * 9);
    }
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$output" "$source"
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
        cat > "$output/color_picker_widget_test.go" <<'GO'
package ziran
import "testing"
type colorPickerHost struct { t *testing.T; swatches, labels int }
func (h *colorPickerHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *colorPickerHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("font") }
    return 12
}
func (h *colorPickerHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 190 ||
       bounds.Y+bounds.Height > 250 { h.t.Fatal("fill bounds") }
    if bounds.Y == 214 && bounds.Height == 36 {
        if color.R != 64 || color.G != 128 || color.B != 191 {
            h.t.Fatal("swatch")
        }
        h.swatches++
    }
}
func (h *colorPickerHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *colorPickerHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *colorPickerHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *colorPickerHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font != 14 || clip.X < 10 || clip.Y < 10 ||
       clip.X+clip.Width > 190 ||
       clip.Y+clip.Height > 250 { h.t.Fatal("label") }
    h.labels++
}
func (h *colorPickerHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("image") }
func TestColorPicker(t *testing.T) {
    h := &colorPickerHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 5; phase++ {
        if ColorPickerWidgetBehavior_Frame() != phase || h.swatches != int(phase+1) ||
           h.labels != int(phase+1)*9 { t.Fatal("phase", phase) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
