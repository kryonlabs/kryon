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
#import "fieldset_props"
#import "fieldset_widget"
#import "geometry"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "widget_kind"

Frame :: () -> i32 #export {
    rules: StyleRules
    rules.count = 1
    rule: StyleRule
    rule.selector = StyleDefaultSelector()
    rule.selector.kind = StyleKindFieldset()
    rule.selector.class_name = 9
    rule.style.fields = (u32)StyleBackground | (u32)StyleForeground |
        (u32)StyleOpacity
    rule.style.background = (u32)0x123456ff
    rule.style.foreground = (u32)0xaabbccff
    rule.style.opacity = 0.5
    rules.items[0] = rule
    InstallStyleRules(rules)
    first: FieldsetProps
    first.key = (u64)7
    first.id = 7
    first.class_name = 9
    first.bounds = (Rectangle){10.0, 30.0, 100.0, 50.0}
    first.title = "Group"
    second: FieldsetProps
    second.key = (u64)8
    second.bounds = (Rectangle){120.0, 30.0, 80.0, 50.0}
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 220.0, 100.0})
    Fieldset(first)
    Fieldset(second)
    if !EndTree() || TreeCount() != 3 ||
        TreeNodeAt(1).kind != WidgetKindFieldset ||
        TreeNodeAt(2).kind != WidgetKindFieldset ||
        TreeNodeAt(1).semantic_label != "Group" ||
        TreeNodeAt(2).semantic_label != "" { return -1 }
    return 7
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
    "$repo/tests/ziran_fieldset_widget_test.c" \
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
    assert(value.length == 5 && memcmp(value.data, "Group", 5) == 0);
    assert(font == 18 && typeface.length == 0);
    return 35;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface;
    return 18;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    if(fills == 0 || fills == 1) {
        assert(color.r == 0x12 && color.g == 0x34 &&
            color.b == 0x56 && color.a == 127);
    }
    if(fills == 1) {
        assert(bounds.x == 18 && bounds.y == 22 &&
            bounds.width == 51 && bounds.height == 18);
        assert(radius == 0 && segments == 4);
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)color;
    assert(radius == 6 && segments == 12 && width == 1);
    outlines++;
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
    assert(value.length == 5 && memcmp(value.data, "Group", 5) == 0);
    assert(x == 26 && y == 21 && font == 18);
    assert(clip.x == 26 && clip.y == 21 &&
        clip.width == 35 && clip.height == 18);
    assert(color.r == 0xaa && color.g == 0xbb &&
        color.b == 0xcc && color.a == 127);
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
    assert(Frame() == 7);
    assert(fills == 3 && outlines == 2 && labels == 1);
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
        cat > "$output/fieldset_widget_test.go" <<'GO'
package ziran
import "testing"
type fieldsetHost struct { t *testing.T; fills, outlines, labels int }
func (h *fieldsetHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Group" || font != 18 || typeface != "" {
        h.t.Fatal("fieldset title measurement")
    }
    return 35
}
func (h *fieldsetHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 18 }
func (h *fieldsetHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if h.fills < 2 &&
       (color.R != 0x12 || color.G != 0x34 ||
        color.B != 0x56 || color.A != 127) { h.t.Fatal("fieldset KSS") }
    if h.fills == 1 &&
       (bounds.X != 18 || bounds.Y != 22 || bounds.Width != 51 ||
        bounds.Height != 18 || radius != 0 || segments != 4) {
        h.t.Fatal("fieldset title cover")
    }
    h.fills++
}
func (h *fieldsetHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if radius != 6 || segments != 12 || width != 1 {
        h.t.Fatal("fieldset border")
    }
    h.outlines++
}
func (h *fieldsetHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *fieldsetHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped title") }
func (h *fieldsetHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Group" || x != 26 || y != 21 || font != 18 ||
       clip.X != 26 || clip.Y != 21 || clip.Width != 35 ||
       clip.Height != 18 || color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 127 { h.t.Fatal("fieldset title") }
    h.labels++
}
func (h *fieldsetHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestFieldsetWidget(t *testing.T) {
    h := &fieldsetHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    if App_Frame() != 7 || h.fills != 3 || h.outlines != 2 || h.labels != 1 {
        t.Fatal("fieldset frame")
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
