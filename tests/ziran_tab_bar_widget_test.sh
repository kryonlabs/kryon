#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "geometry"
#import "semantic"
#import "tab_bar_props"
#import "tab_bar_widget"
#import "tree"
#import "tree_input"

phase: s32;
selected: s32;
scroll_offset: s32;
dragging: bool;
drag_from: s32;
last_clicked_index: s32;
last_clicked_at: float32;
last_click_valid: bool;
selected_bounds: Rectangle;

Click :: (x: float32, y: float32) {
    TreePointerUpdate(PointerFrame.{x, y,
        true, true, false})
    TreePointerUpdate(PointerFrame.{x, y,
        false, false, true})
}

CloseBounds :: () -> Rectangle {
    index: s32 = 0
    while index < TreeCount() {
        if TreeNodeAt(index).semantic_label == "Close tab" {
            return TreeNodeAt(index).bounds
        }
        index += 1
    }
    return Rectangle.{0.0, 0.0, 0.0, 0.0}
}

#program_export
Frame :: () -> s32 {
    tabs: [4]Tab
    tabs[0].key = cast(u64)10
    tabs[0].label = "Home"
    tabs[1].key = cast(u64)11
    tabs[1].label = "Editor"
    tabs[1].closeable = true
    tabs[2].key = cast(u64)12
    tabs[2].label = "Hidden"
    tabs[2].disabled = true
    tabs[3].key = cast(u64)13
    tabs[3].label = "Build"
    tabs[3].closeable = true
    props: TabBarProps
    props.key = cast(u64)77
    props.bounds = Rectangle.{20.0, 20.0, 240.0, 36.0}
    props.selected_index = selected
    props.scroll_offset = scroll_offset
    props.dragging = dragging
    props.drag_from = drag_from
    props.last_clicked_index = last_clicked_index
    props.last_clicked_at = last_clicked_at
    props.last_click_valid = last_click_valid
    props.id = 77
    props.reorder_enabled = true
    props.input.now = cast(float32)phase * 0.1
    if phase == 3 {
        props.input.focused = true
        props.input.right = true
    }
    if phase == 4 {
        props.input.focused = true
        props.input.close = true
    }
    if phase == 5 {
        props.input.middle_click = true
        props.input.middle_x = selected_bounds.x +
            selected_bounds.width * 0.5
        props.input.middle_y = 38.0
    }
    if phase == 12 { props.input.scroll_delta = -40 }
    if phase == 13 { props.focus_selected = true }
    TreeStart(cast(u64)1, Rectangle.{0.0, 0.0, 300.0, 100.0})
    result: TabBarResult = TabBar(props, tabs[0:4])
    if !TreeFinish() || result.node != 1 ||
        TreeNodeAt(result.node).semantic_kind !=
            cast(SemanticKind)SemanticTabList { return -20 }
    if phase == 0 {
        if result.selected_index != 0 ||
            result.activated_index != -1 ||
            TabBarHeight() != 32 { return -1 }
        Click(180.0, 38.0)
    } else if phase == 1 {
        if result.selected_index != 1 ||
            result.activated_index != 1 ||
            result.scroll_offset <= 0 { return -2 }
        close: Rectangle = CloseBounds()
        if close.width <= 0.0 { return -21 }
        Click(close.x + close.width * 0.5,
            close.y + close.height * 0.5)
    } else if phase == 2 {
        if result.closed_index != 1 ||
            result.activated_index != -1 ||
            result.selected_index != 1 { return -3 }
    } else if phase == 3 {
        if result.selected_index != 3 ||
            result.activated_index != 3 ||
            result.scroll_offset <= scroll_offset ||
            result.selected_tab_bounds.x < 20.0 ||
            result.selected_tab_bounds.x +
                result.selected_tab_bounds.width > 260.0 {
            return -4
        }
    } else if phase == 4 {
        if result.closed_index != 3 ||
            result.activated_index != -1 { return -5 }
    } else if phase == 5 {
        if result.middle_clicked_index != 3 ||
            result.selected_index != 3 { return -6 }
    } else if phase == 6 {
        if result.activated_index != -1 { return -7 }
        Click(selected_bounds.x +
            selected_bounds.width * 0.5, 38.0)
    } else if phase == 7 {
        if result.activated_index != 3 ||
            result.double_clicked_index != -1 { return -8 }
        Click(selected_bounds.x +
            selected_bounds.width * 0.5, 38.0)
    } else if phase == 8 {
        if result.double_clicked_index != 3 { return -9 }
        TreePointerUpdate(PointerFrame.{
            selected_bounds.x + selected_bounds.width * 0.5,
            38.0, true, true, false})
    } else if phase == 9 {
        if result.dragging || result.reordered_from != -1 {
            return -10
        }
        TreePointerUpdate(PointerFrame.{-300.0, 38.0,
            true, false, false})
    } else if phase == 10 {
        if !result.dragging || result.drag_from != 3 ||
            result.drag_to != 0 { return -11 }
        TreePointerUpdate(PointerFrame.{-300.0, 38.0,
            false, false, true})
    } else if phase == 11 {
        if result.reordered_from != 3 ||
            result.reordered_to != 0 ||
            result.activated_index != -1 { return -12 }
    } else if phase == 12 {
        if result.scroll_offset != scroll_offset - 40 ||
            result.selected_index != 3 { return -13 }
        Click(100.0, 38.0)
    } else if phase == 13 {
        if result.activated_index != -1 ||
            result.selected_index != 3 ||
            result.scroll_offset <= scroll_offset {
            return -14
        }
    }
    selected = result.selected_index
    scroll_offset = result.scroll_offset
    dragging = result.dragging
    drag_from = result.drag_from
    last_clicked_index = result.last_clicked_index
    last_clicked_at = result.last_clicked_at
    last_click_valid = result.last_click_valid
    selected_bounds = result.selected_tab_bounds
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
    "$repo/tests/ziran_tab_bar_widget_test.c" \
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
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) { (void)face; return (int32_t)value.length * font / 2; }
HOST int32_t MeasureGlyphLineHeight(int32_t font,
    String face) { (void)face; return font; }
HOST String TextSlice(String source, int32_t start, int32_t length) {
    assert(start >= 0 && length >= 0 &&
           (size_t)start + (size_t)length <= source.length);
    String result = {source.data + start, (size_t)length};
    return result;
}
HOST int32_t ImageWidth(String path) { (void)path; return 0; }
HOST int32_t ImageHeight(String path) { (void)path; return 0; }
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius;
    (void)tint;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds,
    float radius, int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width;
    (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    (void)clip;
}
int main(void) {
    for (int phase = 0; phase < 14; phase++)
        assert(Frame() == phase);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/tab_bar_widget_test.go" <<'GO'
package ziran
import "testing"
type tabHost struct{}
func (tabHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (tabHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (tabHost) TextSlice(source string, start,
    length int32) string { return source[start:start+length] }
func TestTabBar(t *testing.T) {
    SetFontMetricsHost(tabHost{})
    for phase := int32(0); phase < 14; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
