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
#import "paned_view"
#import "paned_view_props"
#import "tree"
#import "tree_input"
#import "widget_kind"

using DropZone;
using WidgetKind;

phase: s32;
split: s32;

#program_export
Frame :: () -> s32 {
    bounds: Rectangle = Rectangle.{10.0, 20.0, 120.0, 80.0}
    if PaneDropZone(bounds, Vector2.{70.0, 60.0}, 0.2, 0) !=
            cast(DropZone)DropCenter ||
        PaneDropZone(bounds, Vector2.{12.0, 60.0}, 0.2, 0) !=
            cast(DropZone)DropLeft ||
        PaneDropZone(bounds, Vector2.{129.0, 60.0}, 0.2, 0) !=
            cast(DropZone)DropRight ||
        PaneDropZone(bounds, Vector2.{70.0, 22.0}, 0.2, 0) !=
            cast(DropZone)DropTop ||
        PaneDropZone(bounds, Vector2.{70.0, 98.0}, 0.2, 0) !=
            cast(DropZone)DropBottom { return -15 }
    props: PanedViewProps
    props.key = cast(u64)20
    props.id = 20
    props.bounds = bounds
    props.vertical = phase < 3
    props.has_split = true
    props.split = split
    props.min_first = 20
    props.min_second = 20
    if phase == 0 { props.split = 60 }
    if phase == 3 { props.split = 40 }
    if phase == 6 { props.disabled = true }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 150.0})
    result: PanedViewResult = PanedView(TestSession(), props)
    if !TreeFinish(TestSession()) || TreeCount(TestSession()) != 3 || result.node != 1 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindPanedView ||
        TreeNodeAt(TestSession(), 2).kind != WidgetKindSlider { return -10 }
    if phase == 0 {
        if result.split != 60 || result.changed ||
            result.handle.x != 66.0 ||
            result.first.width != 56.0 ||
            result.second.x != 74.0 ||
            TreeHitAt(TestSession(), 70.0, 30.0) != 2 { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{70.0, 30.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{90.0, 30.0, true, false, false})
    } else if phase == 1 {
        if result.split != 80 || !result.changed ||
            result.handle.x != 86.0 ||
            result.first.width != 76.0 { return -2 }
        TreePointerUpdate(TestSession(), PointerFrame.{180.0, 30.0, false, false, true})
    } else if phase == 2 {
        if result.split != 100 || !result.changed ||
            result.second.x != 114.0 ||
            result.second.width != 16.0 { return -3 }
    } else if phase == 3 {
        if result.split != 40 || result.changed ||
            result.handle.y != 56.0 ||
            result.first.height != 36.0 ||
            result.second.y != 64.0 ||
            TreeHitAt(TestSession(), 20.0, 60.0) != 2 { return -4 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 60.0, true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 80.0, true, false, false})
    } else if phase == 4 {
        if result.split != 60 || !result.changed ||
            result.handle.y != 76.0 { return -5 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 149.0, false, false, true})
    } else if phase == 5 {
        if result.split != 60 || result.changed { return -6 }
    } else {
        if result.split != 60 || result.changed ||
            TreeHitAt(TestSession(), 20.0, 80.0) != -1 { return -7 }
    }
    PaintFlush(TestSession())
    split = result.split
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
    "$repo/tests/ziran_paned_view_widget_test.c" \
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
static int fills;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface; return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface; return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 20 &&
        bounds.x + bounds.width <= 130 &&
        bounds.y + bounds.height <= 100);
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
    (void)clip; RasterText(value, x, y, font, color);
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
int main(void) {
    for (int phase = 0; phase < 7; phase++) {
        assert(Frame() == phase);
    }
    assert(fills == 7);
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
        cat > "$output/paned_view_test.go" <<'GO'
package ziran
import "testing"
type panedHost struct { t *testing.T; fills int }
func (h *panedHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *panedHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 12 }
func (h *panedHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 20 || bounds.X+bounds.Width > 130 ||
       bounds.Y+bounds.Height > 100 { h.t.Fatal("fill outside pane") }
    h.fills++
}
func (h *panedHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *panedHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *panedHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestPanedView(t *testing.T) {
    h := &panedHost{t: t}
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 7; phase++ {
        if App_Frame() != int32(phase) { t.Fatal("pane phase", phase) }
    }
    if h.fills != 7 { t.Fatal("pane fill count", h.fills) }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
