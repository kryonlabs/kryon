#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "drawing_props"
#import "geometry"
#import "overlay_props"
#import "overlay_widget"
#import "paint_queue"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    props: DismissibleOverlayProps
    props.key = (u64)20
    props.bounds = (Rectangle){120.0, 20.0, 80.0, 70.0}
    props.scrim = (Color){0, 0, 0, 160}
    if phase == 5 {
        props.dismiss_disabled = true
        props.view_width = 240
        props.view_height = 120
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 240.0, 120.0})
    beneath: i32 = TreeSubmitCurrent((u64)10,
        WidgetKindButton, (Rectangle){10.0, 10.0, 80.0, 30.0})
    TreeSetInteractive(beneath, false, false, 10)
    overlay: DismissibleOverlayResult = DismissibleOverlay(props)
    if overlay.node < 0 || overlay.panel_node < 0 { return -1 }
    child: i32 = TreeSubmit((u64)3, overlay.panel_node,
        WidgetKindButton, (Rectangle){130.0, 30.0, 50.0, 20.0})
    TreeSetInteractive(child, false, false, 3)
    child_activation: bool = TreeTakeActivationAt(
        TreeFind((u64)3, OverlayPanelKey(), WidgetKindButton))
    beneath_activation: bool = TreeTakeActivationAt(
        TreeFind((u64)10, (u64)1, WidgetKindButton))
    if !TreeFinish() || TreeCount() != 6 ||
        overlay.node != 2 || overlay.panel_node != 4 ||
        TreeHitAt(20.0, 20.0) != 3 ||
        TreeHitAt(135.0, 35.0) != 5 ||
        TreeHitAt(220.0, 110.0) != 3 ||
        beneath_activation { return -2 }
    if phase == 0 {
        if overlay.closed || overlay.release_consumed ||
            child_activation { return -3 }
        TreePointerUpdate((PointerFrame){20.0, 20.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 20.0, false, false, true})
    } else if phase == 1 {
        if !overlay.closed || !overlay.outside_released ||
            !overlay.release_consumed || child_activation { return -4 }
        TreePointerUpdate((PointerFrame){135.0, 35.0, true, true, false})
        TreePointerUpdate((PointerFrame){135.0, 35.0, false, false, true})
    } else if phase == 2 {
        if overlay.closed || overlay.release_consumed ||
            !child_activation { return -5 }
        TreePointerUpdate((PointerFrame){190.0, 80.0, true, true, false})
        TreePointerUpdate((PointerFrame){190.0, 80.0, false, false, true})
    } else if phase == 3 {
        if overlay.closed || overlay.release_consumed ||
            child_activation { return -6 }
        TreePointerUpdate((PointerFrame){190.0, 80.0, true, true, false})
        TreePointerUpdate((PointerFrame){250.0, 130.0, false, false, true})
    } else if phase == 4 {
        if !overlay.closed || !overlay.outside_released ||
            !overlay.release_consumed { return -7 }
        TreePointerUpdate((PointerFrame){20.0, 20.0, true, true, false})
        TreePointerUpdate((PointerFrame){20.0, 20.0, false, false, true})
    } else {
        if overlay.closed || overlay.release_consumed ||
            child_activation { return -8 }
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_overlay_widget_test.c" \
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
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 0 && bounds.y == 0 &&
           bounds.width == 240 && bounds.height == 120);
    assert(radius == 0 && segments == 4 &&
           color.r == 0 && color.g == 0 &&
           color.b == 0 && color.a == 160);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color; assert(0);
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
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    assert(0);
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0);
}
int main(void) {
    for (int phase = 0; phase < 6; phase++) {
        assert(Frame() == phase);
        assert(fills == phase + 1);
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
        cat > "$output/overlay_widget_test.go" <<'GO'
package ziran
import "testing"
type overlayHost struct { t *testing.T; fills int }
func (h *overlayHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 0 || bounds.Y != 0 ||
       bounds.Width != 240 || bounds.Height != 120 ||
       radius != 0 || segments != 4 ||
       color.R != 0 || color.G != 0 ||
       color.B != 0 || color.A != 160 { h.t.Fatal("backdrop") }
    h.fills++
}
func (h *overlayHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *overlayHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *overlayHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *overlayHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) { h.t.Fatal("unexpected text") }
func (h *overlayHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestOverlay(t *testing.T) {
    h := &overlayHost{t: t}
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 6; phase++ {
        if App_Frame() != phase || h.fills != int(phase+1) {
            t.Fatal("overlay phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
