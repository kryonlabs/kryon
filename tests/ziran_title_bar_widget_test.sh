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

#import "control_props"
#import "dropdown_props"
#import "geometry"
#import "paint_queue"
#import "style"
#import "style_sheet"
#import "title_bar"
#import "title_bar_props"
#import "title_bar_widget"
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
        rule.selector.kind = StyleKindTitleBar()
        rule.selector.role = TitleBarBarRole()
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
    props: TitleBarProps
    props.key = cast(u64)10
    props.id = 10
    props.class_name = 9
    props.bounds = Rectangle.{0.0, 0.0, 240.0, 48.0}
    props.title = "Voyager"
    if phase == 4 { props.title = "Interstellar Navigation" }
    props.leading_label = "Return"
    props.has_leading_action = true
    props.has_dropdown = phase > 0 && phase < 4
    props.dropdown.key = cast(u64)11
    props.dropdown.id = 11
    props.dropdown.open = open_state
    props.dropdown.selected_index = selected_state
    props.dropdown.highlight_index = highlight_state
    props.dropdown.scroll_offset = scroll_state
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 260.0, 150.0})
    result: TitleBarResult = TitleBar(TestSession(), props, options[0:2])
    if !TreeFinish(TestSession()) || result.node != 1 ||
        result.height != 48 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindTitleBar ||
        TreeNodeAt(TestSession(), 2).kind != WidgetKindButton ||
        TreeNodeAt(TestSession(), 2).semantic_label != "Return" ||
        TreeNodeAt(TestSession(), 2).bounds.x != 12.0 ||
        TreeNodeAt(TestSession(), 2).bounds.width != 38.0 { return -20 }
    if phase == 0 {
        if result.clicked_leading || TreeCount(TestSession()) != 3 ||
            TreeHitAt(TestSession(), 20.0, 20.0) != 2 { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            false, false, true})
    } else if phase == 1 {
        if !result.clicked_leading || TreeCount(TestSession()) != 4 ||
            result.dropdown.open ||
            TreeNodeAt(TestSession(), 3).kind != WidgetKindDropdown { return -2 }
        TreePointerUpdate(TestSession(), PointerFrame.{70.0, 20.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{70.0, 20.0,
            false, false, true})
    } else if phase == 2 {
        if !result.dropdown.open || !result.dropdown.opened ||
            TreeCount(TestSession()) != 9 { return -3 }
        TreePointerUpdate(TestSession(), PointerFrame.{70.0, 90.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{70.0, 90.0,
            false, false, true})
    } else if phase == 3 {
        if result.dropdown.open || !result.dropdown.changed ||
            result.dropdown.selected_index != 1 ||
            TreeCount(TestSession()) != 4 { return -4 }
    } else if phase == 4 {
        if TreeCount(TestSession()) != 3 || result.clicked_leading ||
            result.dropdown.open { return -5 }
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
    "$repo/tests/ziran_title_bar_widget_test.c" \
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
#include <string.h>
static int fills, strokes, labels, titles;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) {
    assert(font >= 12 && font <= 18 && face.length == 0);
    return (int32_t)(value.length * (size_t)font / 2);
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String face) {
    assert(font >= 12 && font <= 18 && face.length == 0);
    return font;
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
    assert(bounds.x >= 0 && bounds.y >= 0 &&
           bounds.x + bounds.width <= 240 &&
           bounds.y + bounds.height <= 130 && color.a > 0);
    if (bounds.x == 0 && bounds.y == 0 &&
        bounds.width == 240 && bounds.height == 48)
        assert(color.r == 18 && color.g == 52 && color.b == 86);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(line.x >= 0 && line.x + line.width <= 240 &&
           line.y >= 0 && line.y + line.height <= 130 &&
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
    assert(value.length > 0 && font >= 12 && font <= 18 &&
           clip.x >= 0 && clip.x + clip.width <= 240 &&
           color.a > 0);
    if (value.length == 7 && memcmp(value.data, "Voyager", 7) == 0)
        titles++;
    if (value.length == 23 &&
        memcmp(value.data, "Interstellar Navigation", 23) == 0)
        assert(font == 12);
    labels++;
}
int main(void) {
    for (int phase = 0; phase < 5; phase++)
        assert(Frame() == phase);
    assert(fills >= 12 && strokes >= 16 &&
           labels >= 5 && titles == 1);
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
        cat > "$output/title_bar_widget_test.go" <<'GO'
package ziran
import "testing"
type titleHost struct { t *testing.T; fills, strokes, labels, titles int }
func (h *titleHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (h *titleHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (h *titleHost) ImageWidth(path string) int32 {
    h.t.Fatal("unexpected image width"); return 0
}
func (h *titleHost) ImageHeight(path string) int32 {
    h.t.Fatal("unexpected image height"); return 0
}
func (h *titleHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func (h *titleHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 0 || bounds.Y < 0 ||
       bounds.X+bounds.Width > 240 ||
       bounds.Y+bounds.Height > 130 || color.A == 0 {
        h.t.Fatal("fill")
    }
    if bounds.X == 0 && bounds.Y == 0 &&
       bounds.Width == 240 && bounds.Height == 48 &&
       (color.R != 18 || color.G != 52 || color.B != 86) {
        h.t.Fatal("bar KSS")
    }
    h.fills++
}
func (h *titleHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *titleHost) RasterLine(bounds Rectangle, color Color) {
    if bounds.X < 0 || bounds.X+bounds.Width > 240 ||
       bounds.Y < 0 || bounds.Y+bounds.Height > 130 ||
       color.A == 0 { h.t.Fatal("line") }
    h.strokes++
}
func (h *titleHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *titleHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font < 12 || font > 18 || color.A == 0 {
        h.t.Fatal("label")
    }
    if value == "Voyager" { h.titles++ }
    if value == "Interstellar Navigation" && font != 12 {
        h.t.Fatal("title font")
    }
    h.labels++
}
func TestTitleBar(t *testing.T) {
    h := &titleHost{t: t}
    SetFontMetricsHost(h)
    SetImageRasterHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 5; phase++ {
        if App_Frame() != phase { t.Fatal("phase", phase) }
    }
    if h.fills < 12 || h.strokes < 16 ||
       h.labels < 5 || h.titles != 1 { t.Fatal("paint") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
