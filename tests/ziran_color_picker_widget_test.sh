#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "color_picker"
#import "color_picker_props"
#import "color_picker_widget"
#import "drawing_props"
#import "geometry"
#import "paint_queue"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global
compact_red :: float #global
alpha :: float #global

Frame :: () -> i32 #export {
    compact: ColorPickerProps
    compact.key = (u64)20
    compact.bounds = (Rectangle){10.0, 10.0, 180.0, 50.0}
    compact.label = "Color"
    compact.channels = 3
    compact.values[0] = compact_red
    compact.values[1] = 0.5
    compact.values[2] = 0.75
    if phase >= 3 { compact.disabled = true }
    expanded: ColorPickerProps
    expanded.key = (u64)30
    expanded.bounds = (Rectangle){10.0, 70.0, 180.0, 180.0}
    expanded.label = "Preview"
    expanded.channels = 4
    expanded.picker = true
    expanded.values[0] = 0.25
    expanded.values[1] = 0.5
    expanded.values[2] = 0.75
    expanded.values[3] = alpha
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 220.0, 270.0})
    one: ColorPickerResult = ColorPicker(compact)
    two: ColorPickerResult = ColorPicker(expanded)
    if !TreeFinish() || TreeCount() != 19 ||
        one.node != 1 || two.node != 9 ||
        TreeNodeAt(2).kind != WidgetKindText ||
        TreeNodeAt(18).kind != WidgetKindCustom { return -10 }
    red_slider: i32 = TreeFind((u64)1, (u64)20,
        WidgetKindSlider)
    alpha_slider: i32 = TreeFind((u64)4, (u64)30,
        WidgetKindSlider)
    if red_slider < 0 || alpha_slider < 0 { return -14 }
    if TreeNodeAt(red_slider).semantic_label != "R" ||
        TreeNodeAt(alpha_slider).semantic_label != "A" {
        return -15
    }
    swatch: Color = ColorPickerColorFor(two.values[0],
        two.values[1], two.values[2], two.values[3], 4)
    if swatch.r != (u8)64 || swatch.g != (u8)128 ||
        swatch.b != (u8)191 { return -11 }
    if phase == 0 {
        if one.changed || two.changed ||
            one.values[0] != 0.0 { return -1 }
        if TreeHitAt(40.0, 45.0) != red_slider { return -12 }
        TreePointerUpdate((PointerFrame){40.0, 45.0, true, true, false})
        TreePointerUpdate((PointerFrame){65.0, 45.0, true, false, false})
    } else if phase == 1 {
        if !one.changed || one.values[0] < 0.6 ||
            two.changed { return -2 }
        TreePointerUpdate((PointerFrame){65.0, 45.0, false, false, true})
    } else if phase == 2 {
        if one.values[0] < 0.6 || two.changed { return -3 }
        if TreeHitAt(100.0, 190.0) != alpha_slider { return -13 }
        TreePointerUpdate((PointerFrame){100.0, 190.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){175.0, 190.0,
            true, false, false})
    } else if phase == 3 {
        if one.changed || !two.changed ||
            two.values[3] < 0.7 || swatch.a < (u8)178 {
            return -4
        }
        TreePointerUpdate((PointerFrame){175.0, 190.0,
            false, false, true})
        TreePointerUpdate((PointerFrame){40.0, 45.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){65.0, 45.0,
            true, false, false})
    } else {
        if one.changed || one.values[0] != compact_red ||
            two.values[3] != alpha { return -5 }
        TreePointerUpdate((PointerFrame){65.0, 45.0,
            false, false, true})
    }
    PaintFlush()
    compact_red = one.values[0]
    alpha = two.values[3]
    old: i32 = phase
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
    "$repo/tests/ziran_color_picker_widget_test.c" \
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
    "$ziran" build --target="$target" --strict --root "$work" \
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
        if App_Frame() != phase || h.swatches != int(phase+1) ||
           h.labels != int(phase+1)*9 { t.Fatal("phase", phase) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
