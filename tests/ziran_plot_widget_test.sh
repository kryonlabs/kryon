#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

#import "geometry"
#import "paint_queue"
#import "plot"
#import "plot_props"
#import "plot_widget"
#import "tree"
#import "widget_kind"

using PlotMode;
using WidgetKind;

phase: s32;

#program_export
Frame :: () -> s32 {
    lines: [3]float32
    lines[0] = 0.0
    lines[1] = 0.5
    lines[2] = 1.0
    bars: [3]float32
    bars[0] = 0.25
    bars[1] = 0.5
    bars[2] = 1.0
    automatic: PlotProps
    observed: PlotRange = PlotObservedRange(bars[0:3], -1,
        automatic)
    if observed.min_value != 0.25 ||
        observed.max_value != 1.0 ||
        observed.range != 0.75 { return -2 }
    if PlotOffset(0, 5) != 0 || PlotOffset(4, 6) != 2 ||
        PlotOffset(4, -1) != 3 || PlotOffset(4, -6) != 2 ||
        PlotOffset(1, 99) != 0 { return -3 }
    fixed: PlotRange = PlotRangeFor(0.0, 10.0, -1.0, 20.0)
    if fixed.min_value != 0.0 || fixed.max_value != 10.0 ||
        fixed.range != 10.0 || PlotNormalize(5.0, fixed) != 0.5 ||
        PlotNormalize(-5.0, fixed) != 0.0 ||
        PlotNormalize(50.0, fixed) != 1.0 { return -4 }
    singleton: PlotRange = PlotRangeFor(10.0, 5.0, 3.0, 3.0)
    if singleton.min_value != 2.5 ||
        singleton.max_value != 3.5 ||
        singleton.range != 1.0 { return -5 }
    line_props: PlotProps
    line_props.key = cast(u64)11
    line_props.bounds = Rectangle.{10.0, 10.0, 100.0, 60.0}
    line_props.label = "Trend"
    line_props.overlay = "Now"
    line_props.scale_min = 0.0
    line_props.scale_max = 1.0
    bar_props: PlotProps
    bar_props.key = cast(u64)12
    bar_props.bounds = Rectangle.{10.0, 90.0, 100.0, 60.0}
    bar_props.label = "Bars"
    bar_props.overlay = "Peak"
    bar_props.offset = 1
    bar_props.scale_min = 0.0
    bar_props.scale_max = 1.0
    bar_props.mode = cast(PlotMode)PlotBars
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 140.0, 180.0})
    if Plot(TestSession(), line_props, lines[0:3]) != 1 ||
        Plot(TestSession(), bar_props, bars[0:3]) != 2 ||
        !TreeFinish(TestSession()) || TreeCount(TestSession()) != 3 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindPlot ||
        TreeNodeAt(TestSession(), 2).kind != WidgetKindPlot ||
        TreeNodeAt(TestSession(), 1).semantic_label != "Trend" ||
        TreeNodeAt(TestSession(), 2).semantic_label != "Bars" { return -1 }
    PaintFlush(TestSession())
    old: s32 = phase
    phase += 1
    return old
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Frame -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Frame -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_plot_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#define HOST
#endif
static int backgrounds, marks, outlines, lines, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(font == 14 && typeface.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface; return 14;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)segments;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 110 &&
           bounds.y + bounds.height <= 150);
    if (radius == 0) {
        assert(color.r == 59 && color.g == 96 && color.b == 155);
        if (marks % 3 == 0)
            assert(bounds.x > 10.9f && bounds.x < 11.1f &&
                   bounds.y == 120 && bounds.height == 30);
        else if (marks % 3 == 1)
            assert(bounds.x > 44.2f && bounds.x < 44.5f &&
                   bounds.y == 90 && bounds.height == 60);
        else
            assert(bounds.x > 77.5f && bounds.x < 77.8f &&
                   bounds.y == 135 && bounds.height == 15);
        marks++;
    } else {
        assert(radius == 4 && color.r == 245);
        backgrounds++;
    }
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)segments;
    assert(radius == 4 && width == 1 && color.r == 115);
    outlines++;
}
HOST void RasterLine(Rectangle bounds, Color color) {
    assert(bounds.x >= 10 && bounds.x + bounds.width <= 110 &&
           bounds.y >= 10 && bounds.y + bounds.height <= 70);
    assert(color.r == 59 && color.g == 96 && color.b == 155);
    lines++;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color; assert(0);
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)x; (void)y; (void)color;
    assert(value.length > 0 && font == 14 && clip.x == 10 &&
           clip.width == 100 && clip.height == 60);
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
    for (int phase = 0; phase < 2; phase++) {
        assert(Frame() == phase);
        assert(backgrounds == (phase + 1) * 2);
        assert(marks == (phase + 1) * 3);
        assert(outlines == (phase + 1) * 2);
        assert(lines == (phase + 1) * 2);
        assert(labels == (phase + 1) * 4);
    }
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
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
        cat > "$output/plot_widget_test.go" <<'GO'
package ziran
import "testing"
type plotHost struct { t *testing.T; backgrounds, marks, outlines, lines, labels int }
func (h *plotHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("font") }
    return int32(len(value) * 8)
}
func (h *plotHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 14 }
func (h *plotHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 110 ||
       bounds.Y+bounds.Height > 150 { h.t.Fatal("fill bounds") }
    if radius == 0 {
        if color.R != 59 || color.G != 96 || color.B != 155 {
            h.t.Fatal("mark color")
        }
        if h.marks%3 == 0 && (bounds.X < 10.9 || bounds.X > 11.1 ||
           bounds.Y != 120 || bounds.Height != 30) { h.t.Fatal("first bar") }
        if h.marks%3 == 1 && (bounds.X < 44.2 || bounds.X > 44.5 ||
           bounds.Y != 90 || bounds.Height != 60) { h.t.Fatal("second bar") }
        if h.marks%3 == 2 && (bounds.X < 77.5 || bounds.X > 77.8 ||
           bounds.Y != 135 || bounds.Height != 15) { h.t.Fatal("third bar") }
        h.marks++
    } else {
        if radius != 4 || color.R != 245 { h.t.Fatal("background") }
        h.backgrounds++
    }
}
func (h *plotHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if radius != 4 || width != 1 || color.R != 115 { h.t.Fatal("outline") }
    h.outlines++
}
func (h *plotHost) RasterLine(bounds Rectangle, color Color) {
    if bounds.X < 10 || bounds.X+bounds.Width > 110 ||
       bounds.Y < 10 || bounds.Y+bounds.Height > 70 ||
       color.R != 59 { h.t.Fatal("line") }
    h.lines++
}
func (h *plotHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *plotHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font != 14 || clip.X != 10 ||
       clip.Width != 100 || clip.Height != 60 { h.t.Fatal("label") }
    h.labels++
}
func (h *plotHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("image") }
func TestPlot(t *testing.T) {
    h := &plotHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 2; phase++ {
        if App_Frame() != phase || h.backgrounds != int(phase+1)*2 ||
           h.marks != int(phase+1)*3 ||
           h.outlines != int(phase+1)*2 ||
           h.lines != int(phase+1)*2 ||
           h.labels != int(phase+1)*4 { t.Fatal("phase", phase) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
