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
#import "dropdown"
#import "dropdown_props"
#import "dropdown_widget"
#import "geometry"
#import "image_props"
#import "paint_queue"
#import "semantic"
#import "style"
#import "style_sheet"
#import "surface"
#import "tree"
#import "tree_input"
#import "widget_kind"

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
        rule.selector.kind = StyleKindDropdown()
        rule.selector.role = 0
        rule.selector.class_name = 9
        rule.style.fields = cast(u32)StyleBackground
        rule.style.background = cast(u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    options: [3]DropdownOption
    options[0].key = cast(u64)41
    options[0].label = "First"
    options[1].key = cast(u64)42
    options[1].label = "Blocked"
    options[1].disabled = true
    options[2].key = cast(u64)43
    options[2].label = "Third"
    if phase == 13 { options[0].image.asset_path = "badge.png" }
    long_options: [8]DropdownOption
    option_index: s32 = 0
    while option_index < 8 {
        long_options[option_index].key =
            cast(u64)cast(u32)(100 + option_index)
        long_options[option_index].label = "Item"
        option_index += 1
    }
    props: DropdownProps
    props.key = cast(u64)11
    props.id = 11
    props.class_name = 9
    props.bounds = Rectangle.{10.0, 10.0, 120.0, 28.0}
    props.selected_index = selected_state
    props.highlight_index = highlight_state
    props.scroll_offset = scroll_state
    props.open = open_state
    if phase == 4 {
        props.focused = true
        props.trigger_input.down = true
    } else if phase == 5 {
        props.menu_input.home = true
        props.menu_input.navigating = true
    } else if phase == 6 {
        props.menu_input.commit = true
    } else if phase == 7 {
        props.open = true
    } else if phase == 9 {
        props.open = true
        props.disabled = true
    } else if phase >= 10 {
        props.open = true
        if phase == 10 { props.wheel = -1.0 }
        if phase == 13 { props.open = false }
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 220.0, 220.0})
    result: DropdownResult
    if phase >= 10 && phase <= 12 {
        result = Dropdown(TestSession(), props, long_options[0:8])
    } else {
        result = Dropdown(TestSession(), props, options[0:3])
    }
    if !TreeFinish(TestSession()) || result.node != 1 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindDropdown ||
        TreeNodeAt(TestSession(), 1).semantic_kind !=
            cast(SemanticKind)SemanticComboBox { return -20 }
    if phase == 0 {
        if result.open || result.changed || TreeCount(TestSession()) != 2 ||
            TreeHitAt(TestSession(), 20.0, 20.0) != 1 { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 20.0,
            false, false, true})
    } else if phase == 1 {
        if !result.open { return -21 }
        if !result.opened { return -22 }
        if result.highlight_index != 0 { return -23 }
        if TreeCount(TestSession()) != 8 { return -24 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 88.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 88.0,
            false, false, true})
    } else if phase == 2 {
        if !result.open || result.changed ||
            TreeCount(TestSession()) != 8 { return -3 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 115.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 115.0,
            false, false, true})
    } else if phase == 3 {
        if result.open || !result.changed || !result.closed ||
            result.selected_index != 2 || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).semantic_label != "Third" { return -4 }
    } else if phase == 4 {
        if !result.open || !result.opened ||
            result.highlight_index != 2 { return -5 }
    } else if phase == 5 {
        if !result.open || result.highlight_index != 0 ||
            result.changed { return -6 }
    } else if phase == 6 {
        if result.open || !result.closed || !result.changed ||
            result.selected_index != 0 { return -7 }
    } else if phase == 7 {
        if !result.open || TreeCount(TestSession()) != 8 { return -8 }
        TreePointerUpdate(TestSession(), PointerFrame.{180.0, 180.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{180.0, 180.0,
            false, false, true})
    } else if phase == 8 {
        if result.open || !result.closed || result.changed ||
            TreeCount(TestSession()) != 2 { return -9 }
    } else if phase == 9 {
        if result.open || !result.closed ||
            TreeCount(TestSession()) != 2 ||
            TreeHitAt(TestSession(), 20.0, 20.0) != -1 { return -10 }
    } else if phase == 10 {
        if !result.open || result.scroll_offset != 28 ||
            TreeCount(TestSession()) != 11 ||
            TreeNodeAt(TestSession(), 10).kind != WidgetKindSlider ||
            TreeHitAt(TestSession(), 126.0, 80.0) != 10 { return -11 }
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 80.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 160.0,
            true, false, false})
    } else if phase == 11 {
        if !result.open || result.scroll_offset != 84 ||
            TreeCount(TestSession()) != 11 { return -12 }
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 160.0,
            false, false, true})
    } else if phase == 12 {
        if !result.open || result.scroll_offset != 84 ||
            TreeCount(TestSession()) != 11 { return -13 }
    } else if phase == 13 {
        if result.open || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).semantic_label != "First" { return -14 }
    }
    PaintFlush(TestSession())
    open_state = result.open
    selected_state = result.selected_index
    highlight_state = result.highlight_index
    scroll_state = result.scroll_offset
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
    "$repo/tests/ziran_dropdown_widget_test.c" \
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
static int fills, outlines, strokes, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(font == 14 && typeface.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 14;
}
HOST int32_t ImageWidth(String asset_path) {
    assert(asset_path.length == 9); return 32;
}
HOST int32_t ImageHeight(String asset_path) {
    assert(asset_path.length == 9); return 16;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 130 &&
           bounds.y + bounds.height <= 190 && color.a > 0);
    if (bounds.x == 10 && bounds.y == 10 &&
        bounds.width == 120 && bounds.height == 28)
        assert(color.r == 18 && color.g == 52 && color.b == 86);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
           bounds.x + bounds.width <= 130 &&
           bounds.y + bounds.height <= 190 &&
           width == 1 && color.a > 0);
    outlines++;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(line.x >= 10 && line.x + line.width <= 130 &&
           line.y >= 10 && line.y + line.height <= 190 &&
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
    assert(value.length > 0 && font == 14 && clip.x >= 10 &&
           clip.x + clip.width <= 130 && color.a > 0);
    labels++;
}
static int image_draws;
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)origin; (void)rotation; (void)radius;
    assert(path.length == 9 && id == 0 && source.width == 32 &&
           source.height == 16 && destination.x >= 10 &&
           destination.y >= 10 && clip.x == 10 &&
           clip.width == 120 && tint.a == 255);
    image_draws++;
}
int main(void) {
    const int fill_steps[] = {1, 3, 3, 1, 3, 4, 1, 3, 1, 1, 4, 4, 4, 1};
    const int outline_steps[] = {1, 2, 2, 1, 2, 2, 1, 2, 1, 1, 2, 2, 2, 1};
    const int stroke_steps[] = {2, 4, 4, 2, 4, 4, 2, 4, 2, 2, 2, 2, 2, 2};
    const int label_steps[] = {1, 4, 4, 1, 4, 4, 1, 4, 1, 1, 6, 6, 6, 1};
    int expected_fills = 0, expected_outlines = 0;
    int expected_strokes = 0, expected_labels = 0;
    for (int phase = 0; phase < 14; phase++) {
        assert(Frame() == phase);
        expected_fills += fill_steps[phase];
        expected_outlines += outline_steps[phase];
        expected_strokes += stroke_steps[phase];
        expected_labels += label_steps[phase];
        assert(fills == expected_fills && outlines == expected_outlines &&
               strokes == expected_strokes && labels == expected_labels);
        assert(image_draws == (phase == 13 ? 1 : 0));
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
        cat > "$output/dropdown_widget_test.go" <<'GO'
package ziran
import "testing"
type dropdownHost struct { t *testing.T; fills, outlines, strokes, labels, images int }
func (h *dropdownHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value) * 8) }
func (h *dropdownHost) MeasureGlyphLineHeight(font int32,
    face string) int32 {
    if font != 14 || face != "" { h.t.Fatal("font") }
    return 14
}
func (h *dropdownHost) ImageWidth(path string) int32 {
    if path != "badge.png" { h.t.Fatal("image width") }
    return 32
}
func (h *dropdownHost) ImageHeight(path string) int32 {
    if path != "badge.png" { h.t.Fatal("image height") }
    return 16
}
func (h *dropdownHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 ||
       bounds.X+bounds.Width > 130 ||
       bounds.Y+bounds.Height > 190 || color.A == 0 { h.t.Fatal("fill") }
    if bounds.X == 10 && bounds.Y == 10 &&
       bounds.Width == 120 && bounds.Height == 28 &&
       (color.R != 18 || color.G != 52 || color.B != 86) {
        h.t.Fatal("trigger KSS")
    }
    h.fills++
}
func (h *dropdownHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if width != 1 || color.A == 0 { h.t.Fatal("outline") }
    h.outlines++
}
func (h *dropdownHost) RasterLine(bounds Rectangle, color Color) {
    if bounds.X < 10 || bounds.X+bounds.Width > 130 ||
       bounds.Y < 10 || bounds.Y+bounds.Height > 190 ||
       color.A == 0 { h.t.Fatal("line") }
    h.strokes++
}
func (h *dropdownHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *dropdownHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font != 14 || color.A == 0 {
        h.t.Fatal("label")
    }
    h.labels++
}
func (h *dropdownHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    if path != "badge.png" || id != 0 || source.Width != 32 ||
       source.Height != 16 || destination.X < 10 ||
       clip.X != 10 || clip.Width != 120 || tint.A != 255 {
        h.t.Fatal("image")
    }
    h.images++
}
func TestDropdown(t *testing.T) {
    h := &dropdownHost{t: t}
    SetFontMetricsHost(h)
    SetImageRasterHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    fillSteps := []int{1,3,3,1,3,4,1,3,1,1,4,4,4,1}
    outlineSteps := []int{1,2,2,1,2,2,1,2,1,1,2,2,2,1}
    strokeSteps := []int{2,4,4,2,4,4,2,4,2,2,2,2,2,2}
    labelSteps := []int{1,4,4,1,4,4,1,4,1,1,6,6,6,1}
    fills, outlines, strokes, labels := 0, 0, 0, 0
    for phase := int32(0); phase < 14; phase++ {
        if App_Frame() != phase { t.Fatal("phase", phase) }
        fills += fillSteps[phase]
        outlines += outlineSteps[phase]
        strokes += strokeSteps[phase]
        labels += labelSteps[phase]
        if h.fills != fills || h.outlines != outlines ||
           h.strokes != strokes || h.labels != labels {
            t.Fatal("paint", phase, h.fills, h.outlines,
                h.strokes, h.labels)
        }
        if h.images != map[int32]int{13:1}[phase] {
            t.Fatal("images", phase, h.images)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
