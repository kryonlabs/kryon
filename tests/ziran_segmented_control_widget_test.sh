#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "paint_queue"
#import "segmented_control"
#import "segmented_control_props"
#import "segmented_control_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global
selected :: i32 #global

Frame :: () -> i32 #export {
    options: [3]SegmentOption
    options[0] = (SegmentOption){(u64)11, "One", false}
    options[1] = (SegmentOption){(u64)22, "Two", false}
    options[2] = (SegmentOption){(u64)33, "Three", false}
    props: SegmentedControlProps
    props.key = (u64)20
    props.id = 20
    props.bounds = (Rectangle){10.0, 20.0, 150.0, 0.0}
    props.has_selection = true
    props.selected_index = selected
    props.wrap = phase < 4
    if phase == 3 || phase == 4 { options[2].disabled = true }
    if phase == 6 { props.has_selection = false }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 260.0, 120.0})
    result: SegmentedControlResult = SegmentedControl(props, options[0:3])
    if !TreeFinish() || TreeCount() != 5 || result.node != 1 ||
        TreeNodeAt(1).kind != WidgetKindSegmentedControl ||
        TreeNodeAt(2).kind != WidgetKindButton ||
        TreeNodeAt(3).kind != WidgetKindButton ||
        TreeNodeAt(4).kind != WidgetKindButton { return -10 }
    if phase == 0 {
        if result.height != 66 || result.selected_index != 0 ||
            result.changed || result.clicked_index != -1 ||
            TreeNodeAt(2).bounds.x != 10.0 ||
            TreeNodeAt(3).bounds.x != 88.0 ||
            TreeNodeAt(4).bounds.y != 56.0 ||
            TreeHitAt(100.0, 30.0) != 3 { return -1 }
        TreePointerUpdate((PointerFrame){100.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){100.0, 30.0, false, false, true})
    } else if phase == 1 {
        if result.selected_index != 1 || !result.changed ||
            result.clicked_index != 1 { return -2 }
        TreePointerUpdate((PointerFrame){50.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){50.0, 70.0, false, false, true})
    } else if phase == 2 {
        if result.selected_index != 2 || !result.changed ||
            result.clicked_index != 2 { return -3 }
    } else if phase == 3 {
        if result.selected_index != 2 || result.changed ||
            TreeHitAt(50.0, 70.0) != -1 { return -4 }
        TreePointerUpdate((PointerFrame){50.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){50.0, 70.0, false, false, true})
    } else if phase == 4 {
        if result.height != 30 || result.selected_index != 2 ||
            result.changed || TreeNodeAt(4).bounds.x != 166.0 ||
            TreeNodeAt(4).bounds.y != 20.0 { return -5 }
        TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
    } else if phase == 5 {
        if result.selected_index != 0 || !result.changed ||
            result.clicked_index != 0 { return -6 }
    } else {
        if result.selected_index != -1 || result.changed ||
            result.clicked_index != -1 { return -7 }
    }
    PaintFlush()
    selected = result.selected_index
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
    "$repo/tests/ziran_segmented_control_widget_test.c" \
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
static int fills, labels;
HOST int32_t ImageWidth(String path) {
    (void)path; assert(0); return 0;
}
HOST int32_t ImageHeight(String path) {
    (void)path; assert(0); return 0;
}
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(font == 14 && typeface.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 14;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 0 && bounds.y >= 0 &&
        bounds.x + bounds.width <= 260 &&
        bounds.y + bounds.height <= 120);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
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
    (void)value; (void)x; (void)y; (void)color;
    assert(font == 14 && clip.x >= 0 && clip.y >= 0 &&
        clip.x + clip.width <= 260 && clip.y + clip.height <= 120);
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
    for (int phase = 0; phase < 7; phase++) {
        assert(Frame() == phase);
    }
    assert(labels == 21 && fills >= 7);
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
        cat > "$output/segmented_control_test.go" <<'GO'
package ziran
import "testing"
type segmentedHost struct { t *testing.T; fills, labels int }
func (h *segmentedHost) ImageWidth(path string) int32 {
    h.t.Fatal("unexpected image width"); return 0
}
func (h *segmentedHost) ImageHeight(path string) int32 {
    h.t.Fatal("unexpected image height"); return 0
}
func (h *segmentedHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("font width") }
    return int32(len(value)) * 8
}
func (h *segmentedHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("font height") }
    return 14
}
func (h *segmentedHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 0 || bounds.Y < 0 ||
       bounds.X+bounds.Width > 260 || bounds.Y+bounds.Height > 120 {
        h.t.Fatal("fill outside view")
    }
    h.fills++
}
func (h *segmentedHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *segmentedHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *segmentedHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *segmentedHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if font != 14 || clip.X < 0 || clip.Y < 0 ||
       clip.X+clip.Width > 260 || clip.Y+clip.Height > 120 {
        h.t.Fatal("text outside view")
    }
    h.labels++
}
func (h *segmentedHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestSegmentedControl(t *testing.T) {
    h := &segmentedHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    SetImageRasterHost(h)
    for phase := 0; phase < 7; phase++ {
        if App_Frame() != int32(phase) {
            t.Fatal("segment phase", phase)
        }
    }
    if h.labels != 21 || h.fills < 7 {
        t.Fatal("paint count", h.labels, h.fills)
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
