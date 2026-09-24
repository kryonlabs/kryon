#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "checkbox"
#import "checkbox_props"
#import "checkbox_widget"
#import "control_props"
#import "geometry"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    props: CheckboxProps
    props.key = (u64)7
    props.id = 7
    props.class_name = 9
    props.label = "Agree"
    props.bounds = (Rectangle){10.0, 20.0, 120.0, 30.0}
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindCheckbox()
        rule.selector.class_name = 9
        rule.selector.role = CheckboxBoxRoleForTone(
            (ButtonTone)ButtonToneAccent)
        rule.style.fields = (u32)StyleBackground | (u32)StyleBorder
        rule.style.background = (u32)0x123456ff
        rule.style.border = (u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
        BeginTree((u64)1, (Rectangle){0.0, 0.0, 150.0, 100.0})
        result: CheckboxValueResult = Checkbox(props)
        if result.checked || result.changed || !EndTree() ||
            TreeCount() != 2 ||
            TreeNodeAt(1).kind != WidgetKindCheckbox ||
            TreeNodeAt(1).selected ||
            TreeNodeAt(1).semantic_label != "Agree" { return -1 }
        TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
        TreePointerUpdate((PointerFrame){140.0, 80.0, false, false, false})
        phase = 1
        return 0
    }
    if phase == 1 {
        BeginTree((u64)1, (Rectangle){0.0, 0.0, 150.0, 100.0})
        result: CheckboxValueResult = Checkbox(props)
        if !result.checked || !result.changed || !EndTree() ||
            !TreeNodeAt(1).selected {
            return -2
        }
        phase = 2
        return 1
    }
    props.checked = true
    props.disabled = true
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 150.0, 100.0})
    result: CheckboxValueResult = Checkbox(props)
    if !result.checked || result.changed || !EndTree() ||
        !TreeNodeAt(1).selected { return -3 }
    TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
    TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
    phase = 3
    return 2
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
    "$repo/tests/ziran_checkbox_widget_test.c" \
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
static int fills, outlines, lines, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(value.length == 5 && memcmp(value.data, "Agree", 5) == 0);
    assert(font == 14 && typeface.length == 0);
    return 35;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 11 && bounds.y == 25 &&
           bounds.width == 20 && bounds.height == 20);
    assert(radius == 0.18f && segments == 8);
    assert(color.r == 0x12 && color.g == 0x34 && color.b == 0x56);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    assert(bounds.x == 11 && bounds.y == 25 &&
           bounds.width == 20 && bounds.height == 20);
    assert(radius == 0.18f && segments == 8 && width == 1);
    assert(color.r == (outlines == 0 ? 0x73 : 0x12));
    outlines++;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(line.x >= 16 && line.x <= 20 && line.y >= 30 && line.y <= 41);
    assert(line.width > 0 && color.r == 255 && color.g == 255 &&
           color.b == 255);
    lines++;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "Checkbox label must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(value.length == 5 && memcmp(value.data, "Agree", 5) == 0);
    assert(x == 42 && y == 29 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
           clip.width == 120 && clip.height == 30);
    assert(color.r == 0x17 && color.g == 0x17 && color.b == 0x17);
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
    assert(Frame() == 0 && fills == 0 && outlines == 1 &&
           lines == 0 && labels == 1);
    assert(Frame() == 1 && fills == 1 && outlines == 2 &&
           lines == 4 && labels == 2);
    assert(Frame() == 2 && fills == 2 && outlines == 3 &&
           lines == 8 && labels == 3);
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
        cat > "$output/checkbox_widget_test.go" <<'GO'
package ziran
import "testing"
type checkboxHost struct { t *testing.T; fills, outlines, lines, labels int }
func (h *checkboxHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Agree" || font != 14 || typeface != "" {
        h.t.Fatal("checkbox width")
    }
    return 35
}
func (h *checkboxHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("checkbox height") }
    return 12
}
func (h *checkboxHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 11 || bounds.Y != 25 ||
       bounds.Width != 20 || bounds.Height != 20 ||
       radius != 0.18 || segments != 8 ||
       color.R != 0x12 || color.G != 0x34 || color.B != 0x56 {
        h.t.Fatal("checkbox fill")
    }
    h.fills++
}
func (h *checkboxHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if bounds.X != 11 || bounds.Y != 25 ||
       bounds.Width != 20 || bounds.Height != 20 ||
       radius != 0.18 || segments != 8 || width != 1 {
        h.t.Fatal("checkbox outline")
    }
    h.outlines++
}
func (h *checkboxHost) RasterLine(line Rectangle, color Color) {
    if line.X < 16 || line.X > 20 || line.Y < 30 || line.Y > 41 ||
       line.Width <= 0 || color.R != 255 ||
       color.G != 255 || color.B != 255 { h.t.Fatal("checkbox mark") }
    h.lines++
}
func (h *checkboxHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped checkbox label") }
func (h *checkboxHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Agree" || x != 42 || y != 29 || font != 14 ||
       clip.X != 10 || clip.Y != 20 ||
       clip.Width != 120 || clip.Height != 30 ||
       color.R != 0x17 || color.G != 0x17 ||
       color.B != 0x17 { h.t.Fatal("checkbox label") }
    h.labels++
}
func (h *checkboxHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestCheckboxWidget(t *testing.T) {
    h := &checkboxHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    if App_Frame() != 0 || h.fills != 0 || h.outlines != 1 ||
       h.lines != 0 || h.labels != 1 { t.Fatal("unchecked") }
    if App_Frame() != 1 || h.fills != 1 || h.outlines != 2 ||
       h.lines != 4 || h.labels != 2 { t.Fatal("checked") }
    if App_Frame() != 2 || h.fills != 2 || h.outlines != 3 ||
       h.lines != 8 || h.labels != 3 { t.Fatal("disabled") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
