#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "control_props"
#import "geometry"
#import "slider"
#import "slider_props"
#import "slider_widget"
#import "style"
#import "style_sheet"
#import "surface"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    continuous: SliderProps
    continuous.key = (u64)7
    continuous.id = 7
    continuous.class_name = 9
    continuous.label = "Gain"
    continuous.bounds = (Rectangle){10.0, 20.0, 100.0, 80.0}
    continuous.value = 25.0
    continuous.min = 0.0
    continuous.max = 100.0
    discrete: DiscreteSliderProps
    discrete.key = (u64)8
    discrete.id = 8
    discrete.bounds = (Rectangle){130.0, 20.0, 30.0, 100.0}
    discrete.value = 5
    discrete.min = 0
    discrete.max = 10
    discrete.vertical = true
    if phase >= 2 { continuous.value = 100.0 }
    if phase >= 3 {
        discrete.value = 10
        discrete.disabled = true
    }
    if phase == 0 {
        rules: StyleRules
        rules.count = 2
        track: StyleRule
        track.selector = StyleDefaultSelector()
        track.selector.kind = StyleKindSlider()
        track.selector.class_name = 9
        track.selector.role = SliderTrackRole()
        track.style.fields = (u32)StyleBackground
        track.style.background = (u32)0x123456ff
        rules.items[0] = track
        label: StyleRule
        label.selector = StyleDefaultSelector()
        label.selector.kind = StyleKindSlider()
        label.selector.class_name = 9
        label.selector.role = SliderLabelRole()
        label.style.fields = (u32)StyleForeground | (u32)StyleOpacity
        label.style.foreground = (u32)0xaabbccff
        label.style.opacity = 0.5
        rules.items[1] = label
        InstallStyleRules(rules)
    }
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 200.0, 150.0})
    current: SliderStep = Slider(continuous)
    count: SliderDiscreteStep = DiscreteSlider(discrete)
    if !EndTree() || TreeCount() != 3 ||
        TreeNodeAt(1).kind != WidgetKindSlider ||
        TreeNodeAt(2).kind != WidgetKindSlider ||
        TreeNodeAt(1).bounds.height != 80.0 ||
        TreeNodeAt(2).bounds.height != 100.0 ||
        TreeNodeAt(1).semantic_label != "Gain" { return -1 }
    if phase == 0 {
        if current.value != 25.0 || current.changed ||
            count.value != 5 || count.changed { return -2 }
        TreePointerUpdate((PointerFrame){60.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){110.0, 70.0, true, false, false})
        TreePointerUpdate((PointerFrame){110.0, 70.0, false, false, true})
        phase = 1
        return 0
    }
    if phase == 1 {
        if current.value != 100.0 || !current.changed ||
            count.value != 5 || count.changed { return -3 }
        TreePointerUpdate((PointerFrame){145.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){145.0, 0.0, true, false, false})
        TreePointerUpdate((PointerFrame){145.0, 0.0, false, false, true})
        phase = 2
        return 1
    }
    if phase == 2 {
        if current.value != 100.0 || current.changed ||
            count.value != 10 || !count.changed { return -4 }
        phase = 3
        return 2
    }
    if current.value != 100.0 || current.changed ||
        count.value != 10 || count.changed ||
        TreeHitAt(145.0, 70.0) != -1 { return -5 }
    phase = 4
    return 3
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
    "$repo/tests/ziran_slider_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#include <cstring>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#include <string.h>
#define HOST
#endif
static int fills, outlines, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(value.length == 4 && memcmp(value.data, "Gain", 4) == 0);
    assert(font == 14 && typeface.length == 0);
    return 28;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(radius == 0.5f && segments == 16);
    if(fills % 6 == 0) {
        assert(bounds.x == 10 && bounds.y == 69 &&
               bounds.width == 100);
        assert(color.r == 0x12 && color.g == 0x34 && color.b == 0x56);
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    assert(radius == 0.5f && segments == 16 && width == 1);
    assert(bounds.width == 22 && bounds.height == 22);
    assert(color.r == 0x94);
    outlines++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    assert(0 && "unexpected line");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "Slider label must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(value.length == 4 && memcmp(value.data, "Gain", 4) == 0);
    assert(x == 16 && y == 20 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
           clip.width == 88 && clip.height == 12);
    assert(color.r == 0xaa && color.g == 0xbb &&
           color.b == 0xcc && color.a == 127);
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
    for(int phase = 0; phase < 4; phase++) {
        assert(Frame() == phase);
        assert(fills == (phase + 1) * 6 &&
               outlines == (phase + 1) * 2 &&
               labels == phase + 1);
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
        cat > "$output/slider_widget_test.go" <<'GO'
package ziran
import "testing"
type sliderHost struct { t *testing.T; fills, outlines, labels int }
func (h *sliderHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Gain" || font != 14 || typeface != "" {
        h.t.Fatal("slider width")
    }
    return 28
}
func (h *sliderHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("slider height") }
    return 12
}
func (h *sliderHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if radius != 0.5 || segments != 16 { h.t.Fatal("slider fill") }
    if h.fills % 6 == 0 &&
       (bounds.X != 10 || bounds.Y != 69 || bounds.Width != 100 ||
        color.R != 0x12 || color.G != 0x34 || color.B != 0x56) {
        h.t.Fatal("slider KSS track")
    }
    h.fills++
}
func (h *sliderHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if radius != 0.5 || segments != 16 || width != 1 ||
       bounds.Width != 22 || bounds.Height != 22 || color.R != 0x94 {
        h.t.Fatal("slider outline")
    }
    h.outlines++
}
func (h *sliderHost) RasterLine(line Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *sliderHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped slider label") }
func (h *sliderHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Gain" || x != 16 || y != 20 || font != 14 ||
       clip.X != 10 || clip.Y != 20 ||
       clip.Width != 88 || clip.Height != 12 ||
       color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 127 { h.t.Fatal("slider label") }
    h.labels++
}
func (h *sliderHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestSliderWidget(t *testing.T) {
    h := &sliderHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 4; phase++ {
        if App_Frame() != int32(phase) || h.fills != (phase+1)*6 ||
           h.outlines != (phase+1)*2 || h.labels != phase+1 {
            t.Fatal("slider phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
