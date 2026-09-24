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
#import "transition"
#import "transition_widget"

phase :: i32 #global

Frame :: () -> i32 #export {
    state: TransitionState = TransitionStart(1.0)
    color: Color = (Color){10, 20, 30, 200}
    bounds: Rectangle = (Rectangle){2.0, 3.0, 40.0, 20.0}
    if phase == 0 {
        if TransitionFadeColor(state, color).a != (u8)0 { return -1 }
        PaintTransitionFade(state, bounds, color)
    } else if phase == 1 {
        state = TransitionAdvance(state, 0.5).state
        if TransitionFadeColor(state, color).a != (u8)99 { return -2 }
        PaintTransitionFade(state, bounds, color)
    } else if phase == 2 {
        state = TransitionAdvance(state, 1.0).state
        if TransitionFadeColor(state, color).a != (u8)200 { return -3 }
        PaintTransitionFade(state, bounds, color)
        bounds.width = 0.0
        PaintTransitionFade(state, bounds, color)
    } else {
        state = TransitionReset()
        PaintTransitionFade(state, bounds, color)
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_transition_fade_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#define HOST extern "C"
#else
#include "app.h"
#define HOST
#endif
#include <assert.h>
#include <stdint.h>
static int fills;
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 2.0f && bounds.y == 3.0f &&
           bounds.width == 40.0f && bounds.height == 20.0f &&
           radius == 0.0f && segments == 0);
    assert(color.r == 10 && color.g == 20 && color.b == 30);
    assert(color.a == (fills == 0 ? 99 : 200));
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color; assert(0);
}
int main(void) {
    for (int phase = 0; phase < 4; phase++) {
        assert(Frame() == phase);
        assert(fills == (phase == 0 ? 0 : phase == 1 ? 1 : 2));
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
        cat > "$output/transition_fade_widget_test.go" <<'GO'
package ziran
import "testing"
type fadeHost struct { t *testing.T; fills int }
func (h *fadeHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 2 || bounds.Y != 3 || bounds.Width != 40 ||
       bounds.Height != 20 || radius != 0 || segments != 0 ||
       color.R != 10 || color.G != 20 || color.B != 30 { h.t.Fatal("fill") }
    alpha := uint8(200)
    if h.fills == 0 { alpha = 99 }
    if color.A != alpha { h.t.Fatal("alpha", color.A, alpha) }
    h.fills++
}
func (h *fadeHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func TestTransitionFade(t *testing.T) {
    h := &fadeHost{t: t}
    SetRasterShapeHost(h)
    for phase := int32(0); phase < 4; phase++ {
        if App_Frame() != phase || h.fills != map[int32]int{0:0,1:1,2:2,3:2}[phase] {
            t.Fatal("phase", phase)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
