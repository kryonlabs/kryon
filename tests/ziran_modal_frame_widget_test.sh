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
#import "modal"
#import "modal_frame_widget"
#import "modal_props"
#import "paint_queue"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindModal()
        rule.selector.role = ModalPanelRole()
        rule.selector.class_name = 9
        rule.style.fields = (u32)StyleBackground
        rule.style.background = (u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    props: ModalFrameProps
    props.key = (u64)11
    props.id = 11
    props.class_name = 9
    props.title = "Settings"
    if phase == 4 { props.title = "Interstellar Navigation" }
    props.width = 200
    props.height = 140
    props.has_right_action = phase <= 2
    props.right_label = "Close"
    if phase >= 3 {
        props.bounds = (Rectangle){60.0, 20.0, 180.0, 120.0}
        props.has_left_action = true
        props.left_label = "Back"
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 300.0, 220.0})
    result: ModalFrameResult = ModalFrame(props)
    if !TreeFinish() || result.node != 1 ||
        result.panel_node != 3 ||
        TreeNodeAt(1).kind != WidgetKindModal ||
        TreeNodeAt(3).kind != WidgetKindCustom { return -20 }
    if phase == 0 {
        if result.dismissed || result.right_clicked ||
            TreeCount() != 5 ||
            result.layout.panel.x != 50.0 ||
            result.layout.panel.y != 40.0 ||
            result.layout.content.x != 68.0 ||
            TreeNodeAt(4).semantic_label != "Close" ||
            TreeHitAt(220.0, 60.0) != 4 { return -1 }
        TreePointerUpdate((PointerFrame){220.0, 60.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){220.0, 60.0,
            false, false, true})
    } else if phase == 1 {
        if !result.right_clicked || result.dismissed ||
            TreeCount() != 5 { return -2 }
        TreePointerUpdate((PointerFrame){10.0, 10.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){10.0, 10.0,
            false, false, true})
    } else if phase == 2 {
        if !result.dismissed || result.right_clicked ||
            TreeCount() != 4 { return -3 }
    } else if phase == 3 {
        if result.dismissed || result.left_clicked ||
            TreeCount() != 5 ||
            result.layout.panel.x != 60.0 ||
            result.layout.panel.y != 20.0 ||
            result.layout.content.x != 78.0 ||
            TreeNodeAt(4).semantic_label != "Back" ||
            TreeHitAt(80.0, 40.0) != 4 { return -4 }
        TreePointerUpdate((PointerFrame){80.0, 40.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){80.0, 40.0,
            false, false, true})
    } else if phase == 4 {
        if !result.left_clicked || result.dismissed ||
            TreeCount() != 5 { return -5 }
    }
    PaintFlush()
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
    "$repo/tests/ziran_modal_frame_widget_test.c" \
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
static int panels, scrims, labels, strokes;
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
           bounds.x + bounds.width <= 300 &&
           bounds.y + bounds.height <= 220 && color.a > 0);
    if (bounds.width >= 180 && bounds.height >= 120 &&
        bounds.x >= 50 && bounds.y >= 20) {
        assert(color.r == 18 && color.g == 52 && color.b == 86);
        panels++;
    } else if (bounds.x == 0 && bounds.y == 0 &&
        bounds.width == 300 && bounds.height == 220) {
        assert(color.a == 128);
        scrims++;
    }
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(line.x >= 60 && line.x + line.width <= 245 &&
           line.y >= 20 && line.y + line.height <= 80 &&
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
    assert(clip.x >= 50 &&
           clip.x + clip.width <= 250 && color.a > 0);
    if (value.length == 8) {
        assert(memcmp(value.data, "Settings", 8) == 0 && font == 18);
    } else {
        assert(value.length == 23 &&
               memcmp(value.data, "Interstellar Navigation", 23) == 0 &&
               font == 12);
    }
    labels++;
}
int main(void) {
    for (int phase = 0; phase < 5; phase++)
        assert(Frame() == phase);
    assert(panels == 4 && scrims == 5 &&
           labels == 4 && strokes == 8);
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
        cat > "$output/modal_frame_widget_test.go" <<'GO'
package ziran
import "testing"
type modalHost struct { t *testing.T; panels, scrims, labels, strokes int }
func (h *modalHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (h *modalHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (h *modalHost) ImageWidth(path string) int32 {
    h.t.Fatal("unexpected image width"); return 0
}
func (h *modalHost) ImageHeight(path string) int32 {
    h.t.Fatal("unexpected image height"); return 0
}
func (h *modalHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func (h *modalHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 0 || bounds.Y < 0 ||
       bounds.X+bounds.Width > 300 ||
       bounds.Y+bounds.Height > 220 || color.A == 0 {
        h.t.Fatal("fill")
    }
    if bounds.Width >= 180 && bounds.Height >= 120 &&
       bounds.X >= 50 && bounds.Y >= 20 {
        if color.R != 18 || color.G != 52 || color.B != 86 {
            h.t.Fatal("panel KSS")
        }
        h.panels++
    } else if bounds.X == 0 && bounds.Y == 0 &&
       bounds.Width == 300 && bounds.Height == 220 {
        if color.A != 128 { h.t.Fatal("scrim") }
        h.scrims++
    }
}
func (h *modalHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *modalHost) RasterLine(bounds Rectangle, color Color) {
    if bounds.X < 60 || bounds.X+bounds.Width > 245 ||
       bounds.Y < 20 || bounds.Y+bounds.Height > 80 ||
       color.A == 0 { h.t.Fatal("line") }
    h.strokes++
}
func (h *modalHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *modalHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if color.A == 0 ||
       !(value == "Settings" && font == 18 ||
         value == "Interstellar Navigation" && font == 12) {
        h.t.Fatal("title")
    }
    h.labels++
}
func TestModalFrame(t *testing.T) {
    h := &modalHost{t: t}
    SetFontMetricsHost(h)
    SetImageRasterHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 5; phase++ {
        if App_Frame() != phase { t.Fatal("phase", phase) }
    }
    if h.panels != 4 || h.scrims != 5 ||
       h.labels != 4 || h.strokes != 8 { t.Fatal("paint") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
