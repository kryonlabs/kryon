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
#import "spinbox"
#import "spinbox_props"
#import "spinbox_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    props: SpinboxProps
    props.key = (u64)7
    props.id = 7
    props.class_name = 9
    props.label = "Count"
    props.bounds = (Rectangle){10.0, 20.0, 120.0, 36.0}
    props.min = -12
    props.max = 12
    props.step = 2
    props.wrap = true
    props.value = -12
    if phase == 2 { props.value = -10 }
    if phase == 4 {
        props.value = 12
        props.disabled = true
        props.value_text = "twelve"
    }
    if phase == 5 {
        props.value = (i32)(-(i64)2147483647 - (i64)1)
        props.disabled = true
    }
    high: SpinboxStepResult = SpinboxStepValue(2147483646, 0,
        2147483647, 2147483647, 1, false)
    low: SpinboxStepResult = SpinboxStepValue(-2147483647,
        (i32)(-(i64)2147483647 - (i64)1), 0,
        2147483647, -1, false)
    both: SpinboxStepResult = SpinboxStepButtonsValue(4, 0, 10,
        1, true, true, false)
    if high.value != 2147483647 || !high.changed ||
        low.value != (i32)(-(i64)2147483647 - (i64)1) ||
        !low.changed || both.value != 4 || both.changed { return -11 }
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindSpinboxValue()
        rule.selector.class_name = 9
        rule.style.fields = (u32)StyleBackground
        rule.style.background = (u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 180.0, 90.0})
    result: SpinboxStepResult = Spinbox(props)
    if !EndTree() || TreeCount() != 4 ||
        TreeNodeAt(1).kind != WidgetKindSpinbox ||
        TreeNodeAt(1).semantic_label != "Count" ||
        TreeNodeAt(2).parent != 1 || TreeNodeAt(3).parent != 1 ||
        TreeNodeAt(3).focus_id != SpinboxIncrementIdFor(7) {
        return -10
    }
    if phase == 0 {
        if result.value != -12 || result.changed { return -1 }
        TreePointerUpdate((PointerFrame){116.0, 38.0, true, true, false})
        TreePointerUpdate((PointerFrame){116.0, 38.0, false, false, true})
    } else if phase == 1 {
        if result.value != -10 || !result.changed { return -2 }
        TreePointerUpdate((PointerFrame){24.0, 38.0, true, true, false})
        TreePointerUpdate((PointerFrame){24.0, 38.0, false, false, true})
    } else if phase == 2 {
        if result.value != -12 || !result.changed { return -3 }
        TreePointerUpdate((PointerFrame){24.0, 38.0, true, true, false})
        TreePointerUpdate((PointerFrame){24.0, 38.0, false, false, true})
    } else if phase == 3 {
        if result.value != 12 || !result.changed { return -4 }
    } else {
        if result.value != props.value || result.changed ||
            TreeHitAt(116.0, 38.0) != -1 { return -5 }
    }
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_spinbox_widget_test.c" \
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
static int fills;
static char letters[64];
static int length;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(font == 14 && typeface.length == 0);
    return (int32_t)value.length * 7;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments;
    if(fills % 4 == 1) {
        assert(color.r == 0x12 && color.g == 0x34 && color.b == 0x56);
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
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
    (void)x; (void)y; (void)color; (void)clip;
    assert(font == 14 && length + (int)value.length < 64);
    memcpy(letters + length, value.data, value.length);
    length += (int)value.length;
    letters[length] = 0;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
HOST int32_t ImageWidth(String path) { (void)path; return 0; }
HOST int32_t ImageHeight(String path) { (void)path; return 0; }
int main(void) {
    const char *expected[] = {"-+-12", "-+-10", "-+-12", "-+12", "-+twelve", "-+-2147483648"};
    for(int phase = 0; phase < 6; phase++) {
        length = 0;
        assert(Frame() == phase);
        assert(strcmp(letters, expected[phase]) == 0);
        assert(fills == (phase + 1) * 4);
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
        cat > "$output/spinbox_widget_test.go" <<'GO'
package ziran
import "testing"
type spinboxHost struct { t *testing.T; fills int; letters string }
func (h *spinboxHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph width") }
    return int32(len(value) * 7)
}
func (h *spinboxHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *spinboxHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if h.fills % 4 == 1 &&
       (color.R != 0x12 || color.G != 0x34 || color.B != 0x56) {
        h.t.Fatal("KSS SpinboxValue")
    }
    h.fills++
}
func (h *spinboxHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (h *spinboxHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *spinboxHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *spinboxHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) { h.letters += value }
func (h *spinboxHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func (h *spinboxHost) ImageWidth(path string) int32 { return 0 }
func (h *spinboxHost) ImageHeight(path string) int32 { return 0 }
func TestSpinboxWidget(t *testing.T) {
    h := &spinboxHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    expected := []string{"-+-12", "-+-10", "-+-12", "-+12", "-+twelve", "-+-2147483648"}
    for phase := 0; phase < 6; phase++ {
        h.letters = ""
        if App_Frame() != int32(phase) || h.letters != expected[phase] ||
           h.fills != (phase+1)*4 { t.Fatal("spinbox phase", phase, h.letters) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
