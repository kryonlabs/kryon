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
#import "list_box"
#import "list_box_multi"
#import "list_box_props"
#import "list_box_widget"
#import "paint_queue"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global
single_selected :: i32 #global
single_scroll :: i32 #global
multi_scroll :: i32 #global
anchor :: i32 #global
selected :: [4]bool #global

Frame :: () -> i32 #export {
    items: [4]ListBoxItem
    items[0] = (ListBoxItem){(u64)11, "A"}
    items[1] = (ListBoxItem){(u64)12, "B"}
    items[2] = (ListBoxItem){(u64)13, "C"}
    items[3] = (ListBoxItem){(u64)14, "D"}
    single: ListBoxProps
    single.key = (u64)20
    single.bounds = (Rectangle){10.0, 10.0, 100.0, 42.0}
    single.row_height = 20
    single.selected_index = single_selected
    single.scroll_offset = single_scroll
    if phase == 1 { single.scroll_delta = 10 }
    if phase == 2 {
        single.focused = true
        single.navigation = ListBoxKeyEnd()
    }
    if phase >= 3 {
        single.disabled = true
        single.focused = true
        single.navigation = ListBoxKeyHome()
        single.scroll_delta = -100
    }
    multi: ListBoxMultiProps
    multi.key = (u64)30
    multi.bounds = (Rectangle){120.0, 10.0, 100.0, 60.0}
    multi.row_height = 20
    multi.anchor = anchor
    multi.scroll_offset = multi_scroll
    if phase == 0 { multi.anchor = -1 }
    if phase == 2 || phase == 3 { multi.control = true }
    if phase == 4 {
        multi.focused = true
        multi.shift = true
        multi.navigation.home = true
    }
    if phase == 5 {
        multi.disabled = true
        multi.focused = true
        multi.navigation.end = true
        multi.scroll_delta = 100
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 240.0, 100.0})
    one: ListBoxResult = ListBox(single, items[0:4])
    many: ListBoxMultiResult = ListBoxMulti(multi,
        items[0:4], selected[0:4])
    if !TreeFinish() { return -10 }
    if TreeCount() != 13 { return -11 }
    if one.node != 1 || many.node != 7 || !many.valid { return -12 }
    if TreeNodeAt(2).kind != WidgetKindSelectable ||
        TreeNodeAt(6).kind != WidgetKindSlider ||
        TreeNodeAt(12).kind != WidgetKindSlider { return -13 }
    if phase < 2 && TreeNodeAt(2).semantic_label != "A" {
        return -14
    }
    if TreeHitAt(20.0, 53.0) != -1 { return -15 }
    if phase == 0 {
        if one.selected_index != 0 || one.scroll_offset != 0 ||
            one.changed || many.selected_count != 0 ||
            many.anchor != -1 || many.clicked_index != -1 {
            return -1
        }
        TreePointerUpdate((PointerFrame){20.0, 35.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 35.0, false, false, true})
    } else if phase == 1 {
        if one.selected_index != 1 || !one.changed ||
            one.scroll_offset != 10 || many.selected_count != 0 {
            return -2
        }
        TreePointerUpdate((PointerFrame){130.0, 35.0, true, true, false})
        TreePointerUpdate((PointerFrame){130.0, 35.0, false, false, true})
    } else if phase == 2 {
        if one.selected_index != 3 || one.scroll_offset != 38 ||
            !one.changed || !selected[1] || many.selected_count != 1 ||
            many.anchor != 1 || many.clicked_index != 1 {
            return -3
        }
        TreePointerUpdate((PointerFrame){130.0, 55.0, true, true, false})
        TreePointerUpdate((PointerFrame){130.0, 55.0, false, false, true})
    } else if phase == 3 {
        if one.selected_index != 3 || one.changed ||
            one.scroll_offset != 38 { return -41 }
        if many.clicked_index != 2 { return -42 }
        if many.anchor != 2 { return -43 }
        if many.selected_count != 2 || !selected[1] ||
            !selected[2] { return -44 }
    } else if phase == 4 {
        if many.clicked_index != 0 || many.anchor != 0 ||
            many.selected_count != 3 || !selected[0] ||
            !selected[1] || !selected[2] || selected[3] ||
            many.scroll_offset != 0 { return -5 }
    } else if phase == 5 {
        if many.clicked_index != -1 || many.anchor != 0 ||
            many.selected_count != 3 || many.scroll_offset != 0 ||
            one.selected_index != 3 || one.scroll_offset != 38 {
            return -7
        }
        if TreeHitAt(130.0, 35.0) != -1 { return -8 }
        short_selection: [2]bool
        bad: ListBoxMultiResult = ListBoxMulti(multi,
            items[0:4], short_selection[0:2])
        if bad.valid || bad.node != -1 { return -6 }
    }
    PaintFlush()
    single_selected = one.selected_index
    single_scroll = one.scroll_offset
    multi_scroll = many.scroll_offset
    anchor = many.anchor
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
    "$repo/tests/ziran_list_box_widget_test.c" \
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
        if App_Frame() != phase || h.labels != int(phase+1)*6 {
            t.Fatal("phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
