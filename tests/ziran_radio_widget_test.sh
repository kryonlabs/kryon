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
#import "radio"
#import "radio_props"
#import "radio_widget"
#import "style"
#import "style_sheet"
#import "surface"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    if RadioMarkText(true) != "◉" ||
        RadioMarkText(false) != "○" { return -6 }
    first: RadioProps
    first.key = (u64)7
    first.id = 7
    first.class_name = 9
    first.label = "One"
    first.bounds = (Rectangle){10.0, 20.0, 120.0, 30.0}
    first.checked = phase < 2
    second: RadioProps
    second.key = (u64)8
    second.id = 8
    second.class_name = 9
    second.label = "Two"
    second.bounds = (Rectangle){10.0, 60.0, 120.0, 30.0}
    second.checked = phase >= 2
    if phase >= 3 { second.disabled = true }
    if phase == 0 {
        rules: StyleRules
        rules.count = 2
        mark: StyleRule
        mark.selector = StyleDefaultSelector()
        mark.selector.kind = StyleKindRadio()
        mark.selector.class_name = 9
        mark.selector.role = RadioMarkRole()
        mark.style.fields = (u32)StyleBackground | (u32)StyleOpacity
        mark.style.background = (u32)0x123456ff
        mark.style.opacity = 0.5
        rules.items[0] = mark
        label: StyleRule
        label.selector = StyleDefaultSelector()
        label.selector.kind = StyleKindRadio()
        label.selector.class_name = 9
        label.selector.role = RadioLabelRole()
        label.style.fields = (u32)StyleForeground
        label.style.foreground = (u32)0xaabbccff
        rules.items[1] = label
        InstallStyleRules(rules)
    }
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 150.0, 120.0})
    one: i32 = Radio(first)
    two: i32 = Radio(second)
    if !EndTree() || TreeCount() != 3 ||
        TreeNodeAt(1).kind != WidgetKindRadio ||
        TreeNodeAt(1).semantic_label != "One" ||
        TreeNodeAt(2).semantic_label != "Two" ||
        TreeNodeAt(1).bounds.width != 120.0 ||
        TreeNodeAt(2).bounds.width != 120.0 { return -1 }
    if phase == 0 {
        if one != 0 || two != 0 ||
            !TreeNodeAt(1).selected || TreeNodeAt(2).selected {
            return -2
        }
        TreePointerUpdate((PointerFrame){20.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 70.0, false, false, true})
        TreePointerUpdate((PointerFrame){140.0, 110.0, false, false, false})
        phase = 1
        return 0
    }
    if phase == 1 {
        if one != 0 || two != 8 ||
            !TreeNodeAt(1).selected || TreeNodeAt(2).selected {
            return -3
        }
        phase = 2
        return 1
    }
    if phase == 2 {
        if one != 0 || two != 0 ||
            TreeNodeAt(1).selected || !TreeNodeAt(2).selected {
            return -4
        }
        phase = 3
        return 2
    }
    if one != 0 || two != 0 || !TreeNodeAt(2).selected ||
        TreeHitAt(20.0, 70.0) != -1 { return -5 }
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_radio_widget_test.c" \
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
    assert(font == 14 && typeface.length == 0);
    assert((value.length == 3 && memcmp(value.data, "One", 3) == 0) ||
           (value.length == 3 && memcmp(value.data, "Two", 3) == 0));
    return 21;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(radius == 0.5f && segments == 32);
    assert(bounds.x == 22 && bounds.width == 16 && bounds.height == 16);
    assert(bounds.y == (fills < 2 ? 27 : 67));
    assert(color.r == 0x12 && color.g == 0x34 &&
           color.b == 0x56 && color.a == 127);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    assert(radius == 0.5f && segments == 32 && width == 2);
    assert(bounds.x == 20 && bounds.width == 20 && bounds.height == 20);
    assert(bounds.y == (outlines % 2 == 0 ? 25 : 65));
    if(outlines < 4) {
        assert(color.r == (outlines % 2 == 0 ? 0x12 : 0x73));
        assert(color.a == (outlines % 2 == 0 ? 127 : 255));
    } else {
        assert(color.r == (outlines % 2 == 0 ? 0x73 : 0x12));
        assert(color.a == (outlines % 2 == 0 ? 255 : 127));
    }
    outlines++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    assert(0 && "unexpected line");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "Radio label must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(value.length == 3 && x == 58 && font == 14);
    assert(y == (labels % 2 == 0 ? 29 : 69));
    assert(clip.x == 10 && clip.width == 120 && clip.height == 30);
    assert(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc);
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
        assert(fills == phase + 1 && outlines == (phase + 1) * 2 &&
               labels == (phase + 1) * 2);
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
        cat > "$output/radio_widget_test.go" <<'GO'
package ziran
import "testing"
type radioHost struct { t *testing.T; fills, outlines, labels int }
func (h *radioHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if (value != "One" && value != "Two") || font != 14 || typeface != "" {
        h.t.Fatal("radio width")
    }
    return 21
}
func (h *radioHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("radio height") }
    return 12
}
func (h *radioHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    expectedY := float32(27)
    if h.fills >= 2 { expectedY = 67 }
    if radius != 0.5 || segments != 32 || bounds.X != 22 ||
       bounds.Y != expectedY || bounds.Width != 16 ||
       bounds.Height != 16 || color.R != 0x12 ||
       color.G != 0x34 || color.B != 0x56 ||
       color.A != 127 { h.t.Fatal("radio fill") }
    h.fills++
}
func (h *radioHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    expectedY := float32(25)
    if h.outlines % 2 != 0 { expectedY = 65 }
    if radius != 0.5 || segments != 32 || width != 2 ||
       bounds.X != 20 || bounds.Y != expectedY ||
       bounds.Width != 20 || bounds.Height != 20 {
        h.t.Fatal("radio outline")
    }
    selected := (h.outlines < 4 && h.outlines % 2 == 0) ||
        (h.outlines >= 4 && h.outlines % 2 != 0)
    if selected && (color.R != 0x12 || color.A != 127) {
        h.t.Fatal("selected ring KSS")
    }
    if !selected && (color.R != 0x73 || color.A != 255) {
        h.t.Fatal("idle ring")
    }
    h.outlines++
}
func (h *radioHost) RasterLine(line Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *radioHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped radio label") }
func (h *radioHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    expectedY := int32(29)
    if h.labels % 2 != 0 { expectedY = 69 }
    if (value != "One" && value != "Two") || x != 58 ||
       y != expectedY || font != 14 || clip.X != 10 ||
       clip.Width != 120 || clip.Height != 30 ||
       color.R != 0xaa || color.G != 0xbb || color.B != 0xcc {
        h.t.Fatal("radio label")
    }
    h.labels++
}
func (h *radioHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestRadioWidget(t *testing.T) {
    h := &radioHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 4; phase++ {
        if App_Frame() != int32(phase) || h.fills != phase+1 ||
           h.outlines != (phase+1)*2 || h.labels != (phase+1)*2 {
            t.Fatal("radio phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
