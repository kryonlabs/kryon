#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/selectable_widget_behavior.zi
portable=$repo/tests/selectable_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphLineHeight=selectable_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=selectable_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=selectable_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=selectable_widget_host:RasterLine \
    --bind raster_text:RasterText=selectable_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=selectable_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=selectable_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/selectable_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "selectable_widget_behavior.hpp"
#include <cassert>
#include <cstring>
#define HOST extern "C"
#else
#include "selectable_widget_behavior.h"
#include <assert.h>
#include <string.h>
#define HOST
#endif
static int fills, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface;
    return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 10 && bounds.y == 20 &&
        bounds.width == 100 && bounds.height == 36);
    assert(radius == 4 && segments == 12);
    assert(color.r == 0x12 && color.g == 0x34 && color.b == 0x56);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
    assert(0);
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
    assert(value.length == 5 && memcmp(value.data, "Alpha", 5) == 0);
    assert(x == 18 && y == 32 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
        clip.width == 100 && clip.height == 36);
    assert(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc);
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
    const int expected_fills[] = {0, 1, 2, 2};
    for(int phase = 0; phase < 4; phase++) {
        assert(Frame() == phase);
        assert(fills == expected_fills[phase] && labels == phase + 1);
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
        cat > "$output/selectable_widget_test.go" <<'GO'
package ziran
import "testing"
type selectableHost struct { t *testing.T; fills, labels int }
func (h *selectableHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *selectableHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *selectableHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 100 ||
       bounds.Height != 36 || radius != 4 || segments != 12 ||
       color.R != 0x12 || color.G != 0x34 || color.B != 0x56 {
        h.t.Fatal("selectable fill")
    }
    h.fills++
}
func (h *selectableHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *selectableHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *selectableHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *selectableHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Alpha" || x != 18 || y != 32 || font != 14 ||
       clip.X != 10 || clip.Y != 20 || clip.Width != 100 ||
       clip.Height != 36 || color.R != 0xaa ||
       color.G != 0xbb || color.B != 0xcc { h.t.Fatal("selectable label") }
    h.labels++
}
func (h *selectableHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestSelectableWidget(t *testing.T) {
    h := &selectableHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    expectedFills := []int{0, 1, 2, 2}
    for phase := 0; phase < 4; phase++ {
        if SelectableWidgetBehavior_Frame() != int32(phase) || h.fills != expectedFills[phase] ||
           h.labels != phase+1 { t.Fatal("selectable phase", phase) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
