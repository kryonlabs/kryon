#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/list_box_widget_behavior.zi
portable=$repo/tests/list_box_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphLineHeight=list_box_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=list_box_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=list_box_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=list_box_widget_host:RasterLine \
    --bind raster_text:RasterText=list_box_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=list_box_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=list_box_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry list_box_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry list_box_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/list_box_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "list_box_widget_behavior.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "list_box_widget_behavior.h"
#include <assert.h>
#define HOST
#endif
static int labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface;
    assert(0 && "unexpected glyph width");
    return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 220 &&
           bounds.y + bounds.height <= 70);
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments; (void)width; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 220 &&
           bounds.y + bounds.height <= 70);
}
HOST void RasterLine(Rectangle bounds, Color color) {
    (void)bounds; (void)color; assert(0 && "unexpected line");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "unexpected unclipped text");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)x; (void)y; (void)color;
    assert(value.length == 1 && font == 14);
    assert((clip.x >= 10 && clip.x + clip.width <= 110 &&
            clip.y >= 10 && clip.y + clip.height <= 52) ||
           (clip.x >= 120 && clip.x + clip.width <= 220 &&
            clip.y >= 10 && clip.y + clip.height <= 70));
    labels++;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0 && "unexpected image");
}
int main(void) {
    for (int phase = 0; phase < 6; phase++) {
        assert(Frame() == phase);
        assert(labels == (phase + 1) * 6);
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
        cat > "$output/list_box_widget_test.go" <<'GO'
package ziran
import "testing"
type listBoxHost struct { t *testing.T; labels int }
func (h *listBoxHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { h.t.Fatal("unexpected width"); return 0 }
func (h *listBoxHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *listBoxHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 220 ||
       bounds.Y+bounds.Height > 70 { h.t.Fatal("fill bounds") }
}
func (h *listBoxHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 220 ||
       bounds.Y+bounds.Height > 70 { h.t.Fatal("outline bounds") }
}
func (h *listBoxHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *listBoxHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected unclipped text") }
func (h *listBoxHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) != 1 || font != 14 ||
       !((clip.X >= 10 && clip.X+clip.Width <= 110 &&
          clip.Y >= 10 && clip.Y+clip.Height <= 52) ||
         (clip.X >= 120 && clip.X+clip.Width <= 220 &&
          clip.Y >= 10 && clip.Y+clip.Height <= 70)) {
        h.t.Fatal("label clip")
    }
    h.labels++
}
func (h *listBoxHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    h.t.Fatal("unexpected image")
}
func TestListBox(t *testing.T) {
    h := &listBoxHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 6; phase++ {
        if ListBoxWidgetBehavior_Frame() != phase || h.labels != int(phase+1)*6 {
            t.Fatal("phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
