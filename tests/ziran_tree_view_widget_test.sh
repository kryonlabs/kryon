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

#import "geometry"
#import "paint_queue"
#import "semantic"
#import "tree"
#import "tree_input"
#import "tree_view"
#import "tree_view_props"
#import "widget_kind"

using SemanticKind;
using TreeViewNavigation;
using WidgetKind;

phase: s32;
selected: s32;
scroll: s32;

#program_export
Frame :: () -> s32 {
    items: [3]TreeItem
    items[0] = TreeItem.{"First", 0, 1, false, true}
    items[1] = TreeItem.{"Second", 1, 2, true, true}
    items[2] = TreeItem.{"Third", 2, 3, false, true}
    if phase == 8 { items[1].selectable = false }
    props: TreeViewProps
    props.key = cast(u64)20
    props.bounds = Rectangle.{10.0, 10.0, 120.0, 48.0}
    props.row_height = 20
    props.selected_id = selected
    props.scroll_offset = scroll
    if phase == 0 { props.selected_id = 1 }
    if phase == 1 { props.scroll_delta = 12 }
    if phase == 2 {
        props.disabled = true
        props.scroll_delta = 100
    }
    if phase == 3 { props.scroll_offset = 0 }
    if phase >= 6 { props.focused = true }
    if phase == 6 || phase == 8 {
        props.navigation = cast(TreeViewNavigation)TreeViewNavigationUp
    }
    if phase == 7 { props.navigation = cast(TreeViewNavigation)TreeViewNavigationEnd }
    if phase == 9 {
        props.navigation = cast(TreeViewNavigation)TreeViewNavigationHome
        props.disabled = true
    }
    if phase == 10 {
        props.navigation = cast(TreeViewNavigation)TreeViewNavigationDown
    }
    if phase == 11 {
        props.navigation = cast(TreeViewNavigation)TreeViewNavigationHome
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 160.0, 100.0})
    result: TreeViewResult = TreeView(TestSession(), props, items[0:3])
    if !TreeFinish(TestSession()) || TreeCount(TestSession()) != 7 || result.node != 1 ||
        TreeNodeAt(TestSession(), 1).semantic_kind != cast(SemanticKind)SemanticTree ||
        TreeNodeAt(TestSession(), 3).semantic_kind != cast(SemanticKind)SemanticTreeItem ||
        TreeNodeAt(TestSession(), 3).semantic_label != "Second" ||
        TreeNodeAt(TestSession(), 6).kind != WidgetKindSlider { return -10 }
    if phase == 0 {
        if result.selected_id != 1 || result.changed ||
            result.scroll_offset != 0 ||
            TreeNodeAt(TestSession(), 4).bounds.height != 8.0 ||
            TreeHitAt(TestSession(), 20.0, 59.0) != -1 { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 35.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 35.0, false, false, true})
    } else if phase == 1 {
        if result.selected_id != 2 || !result.changed ||
            result.scroll_offset != 12 ||
            TreeNodeAt(TestSession(), 2).bounds.y != 10.0 ||
            TreeNodeAt(TestSession(), 2).bounds.height != 8.0 ||
            TreeHitAt(TestSession(), 20.0, 9.0) != -1 { return -2 }
    } else if phase == 2 {
        if result.selected_id != 2 || result.changed ||
            result.scroll_offset != 12 ||
            TreeHitAt(TestSession(), 20.0, 25.0) != -1 { return -3 }
    } else if phase == 3 {
        if result.selected_id != 2 || result.scroll_offset != 0 ||
            TreeNodeAt(TestSession(), 6).bounds.y != 10.0 ||
            TreeHitAt(TestSession(), 126.0, 20.0) != 6 { return -4 }
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 20.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 40.0, true, false, false})
    } else if phase == 4 {
        if result.selected_id != 2 || result.scroll_offset != 7 ||
            result.changed { return -5 }
        TreePointerUpdate(TestSession(), PointerFrame.{126.0, 45.0, false, false, true})
    } else if phase == 5 {
        if result.selected_id != 2 || result.scroll_offset != 9 ||
            result.changed { return -6 }
    } else if phase == 6 {
        if result.selected_id != 1 || result.scroll_offset != 0 ||
            !result.changed { return -7 }
    } else if phase == 7 {
        if result.selected_id != 3 || result.scroll_offset != 12 ||
            !result.changed { return -8 }
    } else if phase == 8 {
        if result.selected_id != 1 || result.scroll_offset != 0 ||
            !result.changed { return -9 }
    } else if phase == 9 {
        if result.selected_id != 1 || result.scroll_offset != 0 ||
            result.changed { return -11 }
    } else if phase == 10 {
        if result.selected_id != 2 || result.scroll_offset != 0 ||
            !result.changed { return -12 }
    } else {
        if result.selected_id != 1 || result.scroll_offset != 0 ||
            !result.changed { return -13 }
    }
    PaintFlush(TestSession())
    selected = result.selected_id
    scroll = result.scroll_offset
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
    "$repo/tests/ziran_tree_view_widget_test.c" \
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
static int labels;
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
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 10 &&
        bounds.x + bounds.width <= 130 &&
        bounds.y + bounds.height <= 58);
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments; (void)width; (void)color;
    assert(bounds.x == 10 && bounds.y == 10 &&
        bounds.width == 120 && bounds.height == 48);
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
    (void)value; (void)x; (void)y; (void)color;
    assert(font == 14 && clip.x >= 10 && clip.y >= 10 &&
        clip.x + clip.width <= 130 &&
        clip.y + clip.height <= 58);
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
    for(int phase = 0; phase < 12; phase++) {
        assert(Frame() == phase);
        assert(labels == (phase + 1) * 6);
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
        cat > "$output/tree_view_test.go" <<'GO'
package ziran
import "testing"
type treeViewHost struct { t *testing.T; labels int }
func (h *treeViewHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *treeViewHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *treeViewHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 10 || bounds.X+bounds.Width > 130 ||
       bounds.Y+bounds.Height > 58 { h.t.Fatal("fill outside tree") }
}
func (h *treeViewHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if bounds.X != 10 || bounds.Y != 10 || bounds.Width != 120 ||
       bounds.Height != 48 { h.t.Fatal("tree outline") }
}
func (h *treeViewHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *treeViewHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *treeViewHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if font != 14 || clip.X < 10 || clip.Y < 10 ||
       clip.X+clip.Width > 130 || clip.Y+clip.Height > 58 {
        h.t.Fatal("tree label clip")
    }
    h.labels++
}
func (h *treeViewHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestTreeView(t *testing.T) {
    h := &treeViewHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 12; phase++ {
        if App_Frame() != int32(phase) || h.labels != (phase+1)*6 {
            t.Fatal("tree view phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
