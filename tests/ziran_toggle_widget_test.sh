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
#import "style"
#import "style_sheet"
#import "surface"
#import "toggle"
#import "toggle_props"
#import "toggle_widget"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    plain: ToggleProps
    plain.key = (u64)7
    plain.id = 7
    plain.class_name = 9
    plain.label = "Wireless"
    plain.bounds.x = 10.0
    plain.bounds.y = 20.0
    labels: ToggleProps
    labels.key = (u64)8
    labels.id = 8
    labels.class_name = 10
    labels.label = "Mode"
    labels.off_label = "Off"
    labels.on_label = "On"
    labels.bounds.x = 10.0
    labels.bounds.y = 60.0
    if phase == 0 {
        rules: StyleRules
        rules.count = 2
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindToggle()
        rule.selector.class_name = 9
        rule.selector.role = ToggleFillRole()
        rule.style.fields = (u32)StyleBackground
        rule.style.background = (u32)0x123456ff
        rules.items[0] = rule
        label_rule: StyleRule
        label_rule.selector = StyleDefaultSelector()
        label_rule.selector.kind = StyleKindToggle()
        label_rule.selector.class_name = 10
        label_rule.selector.role = ToggleLabelRole()
        label_rule.style.fields = (u32)StyleForeground
        label_rule.style.foreground = (u32)0xaabbccff
        rules.items[1] = label_rule
        InstallStyleRules(rules)
    }
    if phase >= 2 { plain.value = true }
    if phase >= 3 {
        labels.value = true
        labels.disabled = true
    }
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 200.0, 150.0})
    first: ToggleValueResult = Toggle(plain)
    second: ToggleValueResult = Toggle(labels)
    if !EndTree() || TreeCount() != 3 ||
        TreeNodeAt(1).kind != WidgetKindToggle ||
        TreeNodeAt(1).bounds.width != 54.0 ||
        TreeNodeAt(2).bounds.width != 68.0 ||
        TreeNodeAt(1).semantic_label != "Wireless" ||
        TreeNodeAt(2).semantic_label != "Mode" { return -1 }
    if phase == 0 {
        if first.value || first.changed || second.value ||
            second.changed || TreeNodeAt(1).selected ||
            TreeNodeAt(2).selected { return -2 }
        TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
        TreePointerUpdate((PointerFrame){180.0, 140.0, false, false, false})
        phase = 1
        return 0
    }
    if phase == 1 {
        if !first.value || !first.changed || second.value ||
            second.changed || !TreeNodeAt(1).selected ||
            TreeNodeAt(2).selected { return -3 }
        TreePointerUpdate((PointerFrame){20.0, 70.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 70.0, false, false, true})
        TreePointerUpdate((PointerFrame){180.0, 140.0, false, false, false})
        phase = 2
        return 1
    }
    if phase == 2 {
        if !first.value || first.changed || !second.value ||
            !second.changed || !TreeNodeAt(1).selected ||
            !TreeNodeAt(2).selected { return -4 }
        phase = 3
        return 2
    }
    if !first.value || first.changed || !second.value ||
        second.changed || !TreeNodeAt(2).selected ||
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_toggle_widget_test.c" \
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
    if(value.length == 3 && memcmp(value.data, "Off", 3) == 0) return 21;
    assert(value.length == 2 && memcmp(value.data, "On", 2) == 0);
    return 14;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface;
    assert(0 && "Toggle does not measure line height");
    return 0;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(radius == 0.5f && segments == 16);
    if(bounds.x == 10 && bounds.y == 20 && bounds.width == 54) {
        assert(color.r == (fills < 4 ? 0xd1 : 0x12));
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    assert(radius == 0.5f && segments == 16 && width == 1);
    assert(bounds.width == 20 || bounds.width == 24);
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
    assert(0 && "Toggle labels must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(font == 14 && y == 69 && clip.x == 10 && clip.y == 60 &&
           clip.width == 68 && clip.height == 32);
    assert((value.length == 3 && memcmp(value.data, "Off", 3) == 0 &&
            x == 16) ||
           (value.length == 2 && memcmp(value.data, "On", 2) == 0 &&
            x == 54));
    if((labels / 2 < 2 && value.length == 2) ||
       (labels / 2 >= 2 && value.length == 3)) {
        assert(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc);
    }
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
    assert(Frame() == 0 && fills == 4 && outlines == 1 && labels == 2);
    assert(Frame() == 1 && fills == 8 && outlines == 2 && labels == 4);
    assert(Frame() == 2 && fills == 12 && outlines == 3 && labels == 6);
    assert(Frame() == 3 && fills == 16 && outlines == 4 && labels == 8);
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
        cat > "$output/toggle_widget_test.go" <<'GO'
package ziran
import "testing"
type toggleHost struct { t *testing.T; fills, outlines, labels int }
func (h *toggleHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("toggle font") }
    if value == "Off" { return 21 }
    if value == "On" { return 14 }
    h.t.Fatal("toggle text"); return 0
}
func (h *toggleHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { h.t.Fatal("unexpected line height"); return 0 }
func (h *toggleHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if radius != 0.5 || segments != 16 { h.t.Fatal("toggle fill") }
    if bounds.X == 10 && bounds.Y == 20 && bounds.Width == 54 {
        expected := uint8(0xd1)
        if h.fills >= 4 { expected = 0x12 }
        if color.R != expected { h.t.Fatal("toggle KSS track") }
    }
    h.fills++
}
func (h *toggleHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if radius != 0.5 || segments != 16 || width != 1 ||
       (bounds.Width != 20 && bounds.Width != 24) || color.R != 0x94 {
        h.t.Fatal("toggle outline")
    }
    h.outlines++
}
func (h *toggleHost) RasterLine(line Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *toggleHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped toggle label") }
func (h *toggleHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if font != 14 || y != 69 || clip.X != 10 || clip.Y != 60 ||
       clip.Width != 68 || clip.Height != 32 ||
       !((value == "Off" && x == 16) || (value == "On" && x == 54)) {
        h.t.Fatal("toggle label")
    }
    if (h.labels / 2 < 2 && value == "On") ||
       (h.labels / 2 >= 2 && value == "Off") {
        if color.R != 0xaa || color.G != 0xbb || color.B != 0xcc {
            h.t.Fatal("toggle label KSS")
        }
    }
    h.labels++
}
func (h *toggleHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestToggleWidget(t *testing.T) {
    h := &toggleHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 4; phase++ {
        if App_Frame() != int32(phase) || h.fills != (phase+1)*4 ||
           h.outlines != phase+1 || h.labels != (phase+1)*2 {
            t.Fatal("toggle phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
