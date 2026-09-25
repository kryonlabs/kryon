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

#import "dropdown_props"
#import "geometry"
#import "paint_queue"
#import "control_props"
#import "style"
#import "style_sheet"
#import "toolbar"
#import "toolbar_props"
#import "toolbar_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

using WidgetKind;
using StyleField;

phase: s32;
open_state: bool;
selected_state: s32;
highlight_state: s32;
scroll_state: s32;

#program_export
Frame :: () -> s32 {
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindToolbar()
        rule.selector.role = ToolbarActionRole()
        rule.selector.class_name = 9
        rule.style.fields = cast(u32)StyleBackground
        rule.style.background = cast(u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    options: [2]DropdownOption
    options[0].key = cast(u64)41
    options[0].label = "One"
    options[1].key = cast(u64)42
    options[1].label = "Two"
    actions: [2]ToolbarAction
    actions[0].key = cast(u64)51
    actions[0].label = "Act"
    actions[1].key = cast(u64)52
    actions[1].label = "Off"
    actions[1].disabled = true
    props: ToolbarProps
    props.key = cast(u64)10
    props.id = 10
    props.class_name = 9
    props.x = 10
    props.y = 10
    props.width = 300
    props.height = 40
    props.draw_menu = true
    props.dropdown.key = cast(u64)11
    props.dropdown.id = 11
    props.dropdown.selected_index = selected_state
    props.dropdown.highlight_index = highlight_state
    props.dropdown.scroll_offset = scroll_state
    props.dropdown.open = open_state
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 340.0, 180.0})
    result: ToolbarResult = Toolbar(TestSession(), props, options[0:2],
        actions[0:2])
    if !TreeFinish(TestSession()) || result.node != 1 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindToolbar ||
        TreeNodeAt(TestSession(), 2).kind != WidgetKindButton ||
        TreeNodeAt(TestSession(), 3).kind != WidgetKindButton ||
        result.dropdown.node != 4 ||
        TreeNodeAt(TestSession(), 4).kind != WidgetKindDropdown { return -20 }
    if phase == 0 {
        if TreeCount(TestSession()) != 5 || result.clicked_action != -1 ||
            result.selected_menu_item != -1 ||
            TreeHitAt(TestSession(), 230.0, 20.0) != 2 ||
            TreeHitAt(TestSession(), 270.0, 20.0) != -1 { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{230.0, 20.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{230.0, 20.0,
            false, false, true})
    } else if phase == 1 {
        if result.clicked_action != 0 || result.dropdown.open ||
            result.selected_menu_item != -1 { return -2 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            false, false, true})
    } else if phase == 2 {
        if !result.dropdown.open || !result.dropdown.opened ||
            result.clicked_action != -1 || TreeCount(TestSession()) != 10 {
            return -3
        }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 100.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 100.0,
            false, false, true})
    } else if phase == 3 {
        if result.dropdown.open || !result.dropdown.changed ||
            result.dropdown.selected_index != 1 ||
            result.selected_menu_item != 1 ||
            TreeCount(TestSession()) != 5 { return -4 }
    }
    PaintFlush(TestSession())
    open_state = result.dropdown.open
    selected_state = result.dropdown.selected_index
    highlight_state = result.dropdown.highlight_index
    scroll_state = result.dropdown.scroll_offset
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
    "$repo/tests/ziran_toolbar_widget_test.c" \
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
static int fills, strokes, labels, action_fills;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) {
    assert(font == 14 && face.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String face) {
    assert(font == 14 && face.length == 0);
    return 14;
}
HOST int32_t ImageWidth(String path) {
    (void)path; assert(0); return 0;
}
HOST int32_t ImageHeight(String path) {
    (void)path; assert(0); return 0;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius;
    (void)tint; assert(0);
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 310 &&
           bounds.y + bounds.height <= 130 && color.a > 0);
    if (bounds.x == 224 && bounds.y == 13 &&
        bounds.width == 34 && bounds.height == 34) {
        assert(color.r == 18 && color.g == 52 && color.b == 86);
        action_fills++;
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(line.x >= 10 && line.x + line.width <= 310 &&
           line.y >= 10 && line.y + line.height <= 130 &&
           color.a > 0);
    strokes++;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0);
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)x; (void)y;
    assert(value.length > 0 && font == 14 &&
           clip.x >= 10 && clip.x + clip.width <= 310 &&
           color.a > 0);
    labels++;
}
int main(void) {
    for (int phase = 0; phase < 4; phase++)
        assert(Frame() == phase);
    assert(fills >= 16 && strokes >= 12 &&
           labels >= 12 && action_fills == 4);
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
        cat > "$output/toolbar_widget_test.go" <<'GO'
package ziran
import "testing"
type toolbarHost struct { t *testing.T; fills, strokes, labels, actionFills int }
func (h *toolbarHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value) * 8) }
func (h *toolbarHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return 14 }
func (h *toolbarHost) ImageWidth(path string) int32 {
    h.t.Fatal("unexpected image width"); return 0
}
func (h *toolbarHost) ImageHeight(path string) int32 {
    h.t.Fatal("unexpected image height"); return 0
}
func (h *toolbarHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func (h *toolbarHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 310 ||
       bounds.Y+bounds.Height > 130 || color.A == 0 {
        h.t.Fatal("fill")
    }
    if bounds.X == 224 && bounds.Y == 13 &&
       bounds.Width == 34 && bounds.Height == 34 {
        if color.R != 18 || color.G != 52 || color.B != 86 {
            h.t.Fatal("action KSS")
        }
        h.actionFills++
    }
    h.fills++
}
func (h *toolbarHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *toolbarHost) RasterLine(bounds Rectangle, color Color) {
    if bounds.X < 10 || bounds.X+bounds.Width > 310 ||
       bounds.Y < 10 || bounds.Y+bounds.Height > 130 ||
       color.A == 0 { h.t.Fatal("line") }
    h.strokes++
}
func (h *toolbarHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *toolbarHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font != 14 || color.A == 0 {
        h.t.Fatal("label")
    }
    h.labels++
}
func TestToolbar(t *testing.T) {
    h := &toolbarHost{t: t}
    SetFontMetricsHost(h)
    SetImageRasterHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 4; phase++ {
        if App_Frame() != phase { t.Fatal("phase", phase) }
    }
    if h.fills < 16 || h.strokes < 12 ||
       h.labels < 12 || h.actionFills != 4 { t.Fatal("paint") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
