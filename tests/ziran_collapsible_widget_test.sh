#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "collapsible"
#import "collapsible_props"
#import "collapsible_widget"
#import "geometry"
#import "paint_queue"
#import "semantic"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global
open_state :: bool #global
hidden_state :: bool #global

Frame :: () -> i32 #export {
    headers: [4]CollapsibleHeader
    headers[0] = (CollapsibleHeader){19, 0}
    headers[1] = (CollapsibleHeader){20, 1}
    headers[2] = (CollapsibleHeader){21, 2}
    headers[3] = (CollapsibleHeader){22, 1}
    props: CollapsibleProps
    props.key = (u64)20
    props.id = 20
    props.bounds = (Rectangle){10.0, 20.0, 140.0, 90.0}
    props.label = "Details"
    props.close_label = "Close"
    props.open = open_state
    props.hidden = hidden_state
    props.has_open = true
    props.closable = true
    if phase >= 3 && phase <= 5 {
        props.tree = true
        props.depth = 1
        props.closable = false
        props.hidden = false
        props.focused = true
        if phase == 3 {
            props.open = false
            props.navigation = CollapsibleKeyRight()
        } else if phase == 4 {
            props.navigation = CollapsibleKeyRight()
        } else {
            props.open = false
            props.navigation = CollapsibleKeyLeft()
        }
    }
    if phase == 6 {
        props.tree = false
        props.disabled = true
        props.hidden = false
        props.open = false
    }
    if phase == 7 {
        props.tree = false
        props.closable = false
        props.hidden = false
        props.open = false
        props.leaf = true
        props.has_open = false
        props.focused = true
        props.activate = true
        props.selected = true
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 200.0, 150.0})
    result: CollapsibleResult = Collapsible(props, headers[0:4])
    if !TreeFinish() { return -10 }
    if phase == 0 {
        if result.open || result.hidden || result.changed ||
            result.header.height != 32.0 ||
            result.body.width != 112.0 ||
            result.close_bounds.x != 122.0 ||
            result.content.height != 0.0 ||
            TreeCount() != 3 || result.node != 1 ||
            TreeNodeAt(1).kind != WidgetKindCollapsible ||
            TreeNodeAt(1).semantic_kind != (SemanticKind)SemanticButton ||
            TreeNodeAt(2).kind != WidgetKindButton ||
            TreeNodeAt(2).semantic_kind != (SemanticKind)SemanticButton ||
            TreeHitAt(20.0, 30.0) != 1 ||
            TreeHitAt(130.0, 30.0) != 2 { return -1 }
        TreePointerUpdate((PointerFrame){20.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 30.0, false, false, true})
    } else if phase == 1 {
        if !result.open || !result.changed ||
            !result.focus_requested || result.focus_target != 20 ||
            result.content.y != 52.0 ||
            result.content.height != 58.0 { return -2 }
        TreePointerUpdate((PointerFrame){130.0, 30.0, true, true, false})
        TreePointerUpdate((PointerFrame){130.0, 30.0, false, false, true})
    } else if phase == 2 {
        if !result.hidden || !result.closed || !result.changed ||
            result.node != -1 || TreeCount() != 1 { return -3 }
    } else if phase == 3 {
        if !result.open || !result.changed || !result.key_handled ||
            result.focus_requested || result.header.x != 30.0 ||
            result.header.width != 120.0 ||
            TreeNodeAt(1).semantic_kind != (SemanticKind)SemanticTreeItem ||
            TreeCount() != 2 || TreeHitAt(40.0, 30.0) != 1 {
            return -4
        }
    } else if phase == 4 {
        if !result.open || result.changed || !result.key_handled ||
            !result.focus_requested || result.focus_target != 21 ||
            TreeCount() != 2 { return -5 }
    } else if phase == 5 {
        if result.open || result.changed || !result.key_handled ||
            !result.focus_requested || result.focus_target != 19 {
            return -6
        }
    } else if phase == 6 {
        if result.changed || TreeCount() != 3 ||
            TreeHitAt(20.0, 30.0) != -1 ||
            TreeHitAt(130.0, 30.0) != -1 { return -7 }
    } else {
        if result.open || result.changed || result.key_handled ||
            result.content.height != 0.0 || TreeCount() != 2 {
            return -8
        }
    }
    PaintFlush()
    open_state = result.open
    hidden_state = result.hidden
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
    "$repo/tests/ziran_collapsible_widget_test.c" \
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
static int fills, outlines, labels;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(font == 16 && typeface.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 16 && typeface.length == 0);
    return 16;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 0 && bounds.y >= 0 &&
        bounds.x + bounds.width <= 200 &&
        bounds.y + bounds.height <= 150);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments; (void)width; (void)color;
    assert(bounds.x >= 0 && bounds.y >= 0 &&
        bounds.x + bounds.width <= 200 &&
        bounds.y + bounds.height <= 150);
    outlines++;
}
HOST void RasterLine(Rectangle bounds, Color color) {
    (void)bounds; (void)color; assert(0);
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0);
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)color;
    assert(font == 16 && clip.x >= 0 && clip.y >= 0 &&
        clip.x + clip.width <= 200 &&
        clip.y + clip.height <= 150);
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
    for (int phase = 0; phase < 8; phase++) {
        assert(Frame() == phase);
    }
    assert(fills >= 2 && outlines >= 3 && labels >= 12);
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
        cat > "$output/collapsible_test.go" <<'GO'
package ziran
import "testing"
type collapsibleHost struct { t *testing.T; fills, outlines, labels int }
func (h *collapsibleHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 16 || typeface != "" { h.t.Fatal("glyph width") }
    return int32(len(value)) * 8
}
func (h *collapsibleHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 16 || typeface != "" { h.t.Fatal("glyph height") }
    return 16
}
func (h *collapsibleHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 0 || bounds.Y < 0 ||
       bounds.X+bounds.Width > 200 || bounds.Y+bounds.Height > 150 {
        h.t.Fatal("fill outside view")
    }
    h.fills++
}
func (h *collapsibleHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.outlines++
}
func (h *collapsibleHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *collapsibleHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *collapsibleHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if font != 16 || clip.X < 0 || clip.Y < 0 ||
       clip.X+clip.Width > 200 || clip.Y+clip.Height > 150 {
        h.t.Fatal("text outside view")
    }
    h.labels++
}
func (h *collapsibleHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestCollapsible(t *testing.T) {
    h := &collapsibleHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 8; phase++ {
        if App_Frame() != int32(phase) {
            t.Fatal("collapsible phase", phase)
        }
    }
    if h.fills < 2 || h.outlines < 3 || h.labels < 12 {
        t.Fatal("paint count", h.fills, h.outlines, h.labels)
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
