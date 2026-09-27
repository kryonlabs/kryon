#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/paned_view_behavior.zi
portable=$repo/tests/paned_view_portable_test.zi
bind_fill=raster_shape:RasterRoundedRectangle=paned_view_host:RasterRoundedRectangle
bind_outline=raster_shape:RasterRoundedRectangleOutline=paned_view_host:RasterRoundedRectangleOutline
bind_line=raster:RasterLine=paned_view_host:RasterLine
bind_text=raster_text:RasterText=paned_view_host:RasterText
bind_clipped=raster_text:RasterTextClipped=paned_view_host:RasterTextClipped
bind_image=paint_queue:RasterImage=paned_view_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --entry paned_view_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --entry paned_view_portable_test:main \
    -o "$work/saved.zib" "$work/ir/paned_view_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "paned_view_behavior.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "paned_view_behavior.h"
#include <assert.h>
#define HOST
#endif
static int fills;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface; return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface; return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 20 &&
        bounds.x + bounds.width <= 130 &&
        bounds.y + bounds.height <= 100);
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
    (void)clip; RasterText(value, x, y, font, color);
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
int main(void) {
    for (int phase = 0; phase < 7; phase++) {
        assert(Frame() == phase);
    }
    assert(fills == 7);
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
        cat > "$output/paned_view_test.go" <<'GO'
package ziran
import "testing"
type panedHost struct { t *testing.T; fills int }
func (h *panedHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *panedHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 12 }
func (h *panedHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 20 || bounds.X+bounds.Width > 130 ||
       bounds.Y+bounds.Height > 100 { h.t.Fatal("fill outside pane") }
    h.fills++
}
func (h *panedHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *panedHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *panedHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestPanedView(t *testing.T) {
    h := &panedHost{t: t}
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 7; phase++ {
        if PanedViewBehavior_Frame() != int32(phase) { t.Fatal("pane phase", phase) }
    }
    if h.fills != 7 { t.Fatal("pane fill count", h.fills) }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
