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
#import "image_props"
#import "navigation_bar_props"
#import "navigation_bar_widget"
#import "paint_queue"
#import "semantic"
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
        rule.selector.kind = StyleKindNavigationBar()
        rule.selector.class_name = 9
        rule.style.fields = (u32)StyleBackground
        rule.style.background = (u32)0x123456ff
        rules.items[0] = rule
        InstallStyleRules(rules)
    }
    items: [3]NavigationBarItem
    items[0].key = (u64)41
    items[0].route = 11
    items[0].label = "Home"
    items[0].image.asset_path = "icon.png"
    items[0].active = true
    items[1].key = (u64)42
    items[1].route = 22
    items[1].label = "Search"
    items[1].image.asset_path = "icon.png"
    items[2].key = (u64)43
    items[2].route = 33
    items[2].label = "Blocked"
    items[2].disabled = true
    if phase == 1 {
        first: NavigationBarItem = items[0]
        items[0] = items[1]
        items[1] = first
    }
    if phase == 2 { items[1].disabled = true }
    props: NavigationBarProps
    props.key = (u64)10
    props.id = 10
    props.class_name = 9
    props.view_width = 300
    props.view_height = 180
    props.height = 80
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 300.0, 180.0})
    result: NavigationBarResult = NavigationBar(props,
        items[0:3])
    if !TreeFinish() || result.node != 1 ||
        result.y != 100 || result.height != 80 ||
        TreeCount() != 5 ||
        TreeNodeAt(1).kind != WidgetKindNavigationBar ||
        TreeNodeAt(2).kind != WidgetKindButton ||
        TreeNodeAt(3).kind != WidgetKindButton ||
        TreeNodeAt(4).kind != WidgetKindButton ||
        TreeNodeAt(2).semantic_kind !=
            (SemanticKind)SemanticButton { return -20 }
    if phase == 0 {
        if result.clicked_index != -1 ||
            TreeHitAt(150.0, 120.0) != 3 ||
            TreeHitAt(250.0, 120.0) != -1 { return -1 }
        TreePointerUpdate((PointerFrame){150.0, 120.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){150.0, 120.0,
            false, false, true})
    } else if phase == 1 {
        if result.clicked_index != 0 ||
            result.clicked_route != 22 ||
            TreeNodeAt(2).semantic_label != "Search" ||
            TreeHitAt(250.0, 120.0) != -1 { return -2 }
    } else if phase == 2 {
        if result.clicked_index != -1 ||
            TreeHitAt(150.0, 120.0) != -1 { return -3 }
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
    "$repo/tests/ziran_navigation_bar_widget_test.c" \
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
static int fills, outlines, labels, images, bar_fills;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) {
    assert(font == 12 && face.length == 0);
    return (int32_t)value.length * 7;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String face) {
    assert(font == 12 && face.length == 0);
    return 12;
}
HOST int32_t ImageWidth(String path) {
    assert(path.length == 8 && memcmp(path.data, "icon.png", 8) == 0);
    return 24;
}
HOST int32_t ImageHeight(String path) {
    assert(path.length == 8 && memcmp(path.data, "icon.png", 8) == 0);
    return 24;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)origin; (void)rotation; (void)radius;
    assert(path.length == 8 && id == 0 && source.width == 24 &&
           source.height == 24 && destination.y >= 100 &&
           clip.y == 100 && clip.height == 80 && tint.a > 0);
    images++;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 0 && bounds.y >= 100 &&
           bounds.x + bounds.width <= 300 &&
           bounds.y + bounds.height <= 180 && color.a > 0);
    if (bounds.x == 1 && bounds.y == 101 &&
        bounds.width == 298 && bounds.height == 78) {
        assert(color.r == 18 && color.g == 52 && color.b == 86);
        bar_fills++;
    }
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments;
    assert(bounds.x >= 0 && bounds.y >= 100 && width == 1 &&
           color.a > 0);
    outlines++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color; assert(0);
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0);
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)x; (void)y;
    assert(value.length > 0 && font == 12 &&
           clip.y >= 100 && clip.y + clip.height <= 180 &&
           color.a > 0);
    labels++;
}
int main(void) {
    for (int phase = 0; phase < 3; phase++)
        assert(Frame() == phase);
    assert(fills >= 6 && outlines == 3 &&
           labels == 9 && images == 6 && bar_fills == 3);
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
        cat > "$output/navigation_bar_widget_test.go" <<'GO'
package ziran
import "testing"
type navHost struct { t *testing.T; fills, outlines, labels, images, barFills int }
func (h *navHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * 7 }
func (h *navHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return 12 }
func (h *navHost) ImageWidth(path string) int32 {
    if path != "icon.png" { h.t.Fatal("image width") }
    return 24
}
func (h *navHost) ImageHeight(path string) int32 {
    if path != "icon.png" { h.t.Fatal("image height") }
    return 24
}
func (h *navHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    if path != "icon.png" || id != 0 || source.Width != 24 ||
       source.Height != 24 || destination.Y < 100 ||
       clip.Y != 100 || clip.Height != 80 || tint.A == 0 {
        h.t.Fatal("image")
    }
    h.images++
}
func (h *navHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 0 || bounds.Y < 100 ||
       bounds.X+bounds.Width > 300 ||
       bounds.Y+bounds.Height > 180 || color.A == 0 {
        h.t.Fatal("fill")
    }
    if bounds.X == 1 && bounds.Y == 101 &&
       bounds.Width == 298 && bounds.Height == 78 {
        if color.R != 18 || color.G != 52 || color.B != 86 {
            h.t.Fatal("bar KSS")
        }
        h.barFills++
    }
    h.fills++
}
func (h *navHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if width != 1 || color.A == 0 { h.t.Fatal("outline") }
    h.outlines++
}
func (h *navHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *navHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *navHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if len(value) == 0 || font != 12 || color.A == 0 {
        h.t.Fatal("label")
    }
    h.labels++
}
func TestNavigationBar(t *testing.T) {
    h := &navHost{t: t}
    SetFontMetricsHost(h)
    SetImageRasterHost(h)
    SetRasterShapeHost(h)
    SetRasterHost(h)
    SetRasterTextHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 3; phase++ {
        if App_Frame() != phase { t.Fatal("phase", phase) }
    }
    if h.fills < 6 || h.outlines != 3 ||
       h.labels != 9 || h.images != 6 ||
       h.barFills != 3 { t.Fatal("paint") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
