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
#import "geometry"
#import "selectable"
#import "selectable_props"
#import "selectable_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase: s32;

#program_export
Frame :: () -> s32 {
    props: SelectableProps
    props.key = cast(u64)7
    props.id = 7
    props.class_name = 9
    props.bounds = Rectangle.{10.0, 20.0, 100.0, 36.0}
    props.label = "Alpha"
    if phase == 2 { props.selected = true }
    if phase == 3 { props.disabled = true }
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindSelectable()
        rule.selector.class_name = 9
        rule.style.fields = cast(u32)StyleBackground | cast(u32)StyleForeground
        rule.style.background = cast(u32)0x123456ff
        rule.style.foreground = cast(u32)0xaabbccff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 140.0, 80.0})
    result: SelectableToggleResult = Selectable(TestSession(), props)
    if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindSelectable ||
        TreeNodeAt(TestSession(), 1).semantic_label != "Alpha" ||
        TreeNodeAt(TestSession(), 1).selected != result.selected { return -10 }
    if phase == 0 {
        if result.selected || result.changed { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{50.0, 38.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{50.0, 38.0, false, false, true})
    } else if phase == 1 {
        if !result.selected || !result.changed { return -2 }
        TreePointerUpdate(TestSession(), PointerFrame.{50.0, 38.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{50.0, 38.0, false, false, true})
    } else if phase == 2 {
        if result.selected || !result.changed { return -3 }
    } else {
        if result.selected || result.changed ||
            TreeHitAt(TestSession(), 50.0, 38.0) != -1 { return -4 }
    }
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
    "$repo/tests/ziran_selectable_widget_test.c" \
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
    (void)value; (void)font; (void)typeface;
    return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 10 && bounds.y == 20 &&
        bounds.width == 100 && bounds.height == 36);
    assert(radius == 4 && segments == 12);
    assert(color.r == 0x12 && color.g == 0x34 && color.b == 0x56);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
    assert(0);
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
    assert(value.length == 5 && memcmp(value.data, "Alpha", 5) == 0);
    assert(x == 18 && y == 32 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
        clip.width == 100 && clip.height == 36);
    assert(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc);
    labels++;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
int main(void) {
    const int expected_fills[] = {0, 1, 2, 2};
    for(int phase = 0; phase < 4; phase++) {
        assert(Frame() == phase);
        assert(fills == expected_fills[phase] && labels == phase + 1);
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
        cat > "$output/selectable_widget_test.go" <<'GO'
package ziran
import "testing"
type selectableHost struct { t *testing.T; fills, labels int }
func (h *selectableHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *selectableHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *selectableHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 100 ||
       bounds.Height != 36 || radius != 4 || segments != 12 ||
       color.R != 0x12 || color.G != 0x34 || color.B != 0x56 {
        h.t.Fatal("selectable fill")
    }
    h.fills++
}
func (h *selectableHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *selectableHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *selectableHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *selectableHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Alpha" || x != 18 || y != 32 || font != 14 ||
       clip.X != 10 || clip.Y != 20 || clip.Width != 100 ||
       clip.Height != 36 || color.R != 0xaa ||
       color.G != 0xbb || color.B != 0xcc { h.t.Fatal("selectable label") }
    h.labels++
}
func (h *selectableHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestSelectableWidget(t *testing.T) {
    h := &selectableHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    expectedFills := []int{0, 1, 2, 2}
    for phase := 0; phase < 4; phase++ {
        if App_Frame() != int32(phase) || h.fills != expectedFills[phase] ||
           h.labels != phase+1 { t.Fatal("selectable phase", phase) }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
