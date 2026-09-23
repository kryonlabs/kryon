#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "button_props"
#import "button_widget"
#import "control_props"
#import "geometry"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "tree_input"

phase :: i32 #global

Frame :: () -> i32 #export {
    props: ButtonProps
    props.key = (u64)7
    props.id = 7
    props.class_name = 9
    props.label = "Run"
    props.bounds = (Rectangle){10.0, 20.0, 80.0, 30.0}
    props.tone = (ButtonTone)ButtonToneAccent
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindButton()
        rule.selector.class_name = 9
        rule.style.fields = (u32)StyleBackground |
            (u32)StyleForeground | (u32)StyleRadius |
            (u32)StyleOpacity | (u32)StyleFontSize
        rule.style.background = (u32)0x123456ff
        rule.style.foreground = (u32)0xaabbccff
        rule.style.radius = 4.0
        rule.style.opacity = 0.5
        rule.style.font_size = 14.0
        rules.items[0] = rule
        InstallStyleRules(rules)
        BeginTree((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
        if Button(props) != 0 || !EndTree() || TreeCount() != 2 ||
            TreeNodeAt(1).semantic_label != "Run" { return -1 }
        TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
        phase = 1
        return 0
    }
    if phase == 1 {
        TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
        BeginTree((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
        clicked: i32 = Button(props)
        if !EndTree() || clicked != 1 { return -2 }
        phase = 2
        return 42
    }
    props.disabled = true
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    clicked: i32 = Button(props)
    if !EndTree() || clicked != 0 { return -3 }
    TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
    TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
    phase = 3
    return 43
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
    "$repo/tests/ziran_button_widget_test.c" \
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
static int fills, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(value.length == 3 && memcmp(value.data, "Run", 3) == 0);
    assert(font == 14 && typeface.length == 0);
    return 21;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 10 && bounds.y == 20 &&
           bounds.width == 80 && bounds.height == 30);
    assert(radius == 4 && segments == 12);
    assert(color.r == 0x12 && color.g == 0x34 &&
           color.b == 0x56 && color.a == 127);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
    assert(0 && "unexpected outline");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "Button label must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(value.length == 3 && memcmp(value.data, "Run", 3) == 0);
    assert(x == 39 && y == 29 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
           clip.width == 80 && clip.height == 30);
    assert(color.r == 0xaa && color.g == 0xbb &&
           color.b == 0xcc && color.a == 127);
    labels++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    assert(0 && "unexpected line");
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0 && "unexpected image");
}
int main(void) {
    assert(Frame() == 0 && fills == 1 && labels == 1);
    assert(Frame() == 42 && fills == 2 && labels == 2);
    assert(Frame() == 43 && fills == 3 && labels == 3);
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
        cat > "$output/button_widget_test.go" <<'GO'
package ziran
import "testing"
type buttonTestHost struct { t *testing.T; fills, labels int }
func (h *buttonTestHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Run" || font != 14 || typeface != "" {
        h.t.Fatal("button width")
    }
    return 21
}
func (h *buttonTestHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("button height") }
    return 12
}
func (h *buttonTestHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 80 ||
       bounds.Height != 30 || radius != 4 || segments != 12 ||
       color.R != 0x12 || color.G != 0x34 ||
       color.B != 0x56 || color.A != 127 { h.t.Fatal("button fill") }
    h.fills++
}
func (h *buttonTestHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *buttonTestHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped button label") }
func (h *buttonTestHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Run" || x != 39 || y != 29 || font != 14 ||
       clip.X != 10 || clip.Y != 20 ||
       clip.Width != 80 || clip.Height != 30 ||
       color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 127 { h.t.Fatal("button label") }
    h.labels++
}
func (h *buttonTestHost) RasterLine(line Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *buttonTestHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    h.t.Fatal("unexpected image")
}
func TestButtonWidget(t *testing.T) {
    h := &buttonTestHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    if App_Frame() != 0 || h.fills != 1 || h.labels != 1 {
        t.Fatal("first frame")
    }
    if App_Frame() != 42 || h.fills != 2 || h.labels != 2 {
        t.Fatal("activation")
    }
    if App_Frame() != 43 || h.fills != 3 || h.labels != 3 {
        t.Fatal("disabled")
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
