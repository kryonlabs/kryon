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
#import "toast"
#import "toast_props"
#import "toast_widget"
#import "tree"
#import "widget_kind"

phase: s32;
state: ToastState;

#program_export
Frame :: () -> s32 {
    if ToastPrefixBefore("aéz", 3) != 1 { return -20 }
    utf8: ToastDisplay = ToastDisplayFor("aéabcdefghijkl", 48,
        14, "")
    if !utf8.ellipsis || utf8.prefix != "aé" ||
        utf8.width != 48 { return -21 }
    props: ToastProps
    props.key = cast(u64)17
    props.viewport = Rectangle.{10.0, 20.0, 200.0, 100.0}
    props.state = state
    if phase == 0 {
        props.message = "Hello"
        props.seconds = 2.0
        props.now_seconds = 1.0
    } else if phase == 1 {
        props.now_seconds = 2.0
    } else if phase == 2 {
        props.message = "1234567890123456789012345"
        props.seconds = 1.0
        props.now_seconds = 2.5
    } else if phase == 3 {
        props.now_seconds = 4.0
    } else if phase == 4 {
        props.message = "X"
        props.now_seconds = 5.0
    } else {
        props.clear = true
        props.now_seconds = 6.0
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 220.0, 140.0})
    result: ToastResult = Toast(TestSession(), props)
    if !TreeFinish(TestSession()) { return -10 }
    if phase == 0 || phase == 1 {
        if !result.visible || result.state.until_seconds != 3.0 ||
            result.display != "Hello" || result.ellipsis ||
            result.bounds.x != 76.0 || result.bounds.y != 68.0 ||
            TreeCount(TestSession()) != 2 || result.node != 1 ||
            TreeNodeAt(TestSession(), 1).kind != WidgetKindCustom { return -1 }
    } else if phase == 2 {
        if !result.visible || result.state.until_seconds != 3.5 ||
            !result.ellipsis || result.display.count != 14 ||
            result.bounds.width != 164.0 || TreeCount(TestSession()) != 2 {
            return -2
        }
    } else if phase == 3 || phase == 5 {
        if result.visible || result.state.message.count != 0 ||
            TreeCount(TestSession()) != 1 { return -3 }
    } else {
        if !result.visible || result.state.until_seconds != 8.0 ||
            result.display != "X" || TreeCount(TestSession()) != 2 { return -4 }
    }
    PaintFlush(TestSession())
    state = result.state
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
    "$repo/tests/ziran_toast_widget_test.c" \
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
    assert(font == 14 && typeface.length == 0);
    return (int32_t)value.length * 8;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 14;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 20 &&
        bounds.x + bounds.width <= 210 &&
        bounds.y + bounds.height <= 120);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)radius; (void)segments; (void)width; (void)color;
    assert(bounds.x >= 10 && bounds.y >= 20 &&
        bounds.x + bounds.width <= 210 &&
        bounds.y + bounds.height <= 120);
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
    (void)x; (void)y; (void)color;
    assert(font == 14 && clip.x >= 10 && clip.y >= 20 &&
        clip.x + clip.width <= 210 &&
        clip.y + clip.height <= 120);
    if (labels == 0 || labels == 1) {
        assert(value.length == 5 && memcmp(value.data, "Hello", 5) == 0);
    } else if (labels == 2) {
        assert(value.length == 14 &&
            memcmp(value.data, "12345678901234", 14) == 0);
    } else if (labels == 3) {
        assert(value.length == 3 && memcmp(value.data, "...", 3) == 0);
    } else {
        assert(labels == 4 && value.length == 1 && value.data[0] == 'X');
    }
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
    for (int phase = 0; phase < 6; phase++) {
        assert(Frame() == phase);
    }
    assert(fills == 4 && outlines == 4 && labels == 5);
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
        cat > "$output/toast_test.go" <<'GO'
package ziran
import "testing"
type toastHost struct { t *testing.T; fills, outlines, labels int }
func (h *toastHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("font width") }
    return int32(len(value)) * 8
}
func (h *toastHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("line height") }
    return 14
}
func (h *toastHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 20 ||
       bounds.X+bounds.Width > 210 || bounds.Y+bounds.Height > 120 {
        h.t.Fatal("fill outside view")
    }
    h.fills++
}
func (h *toastHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.outlines++
}
func (h *toastHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *toastHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *toastHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if font != 14 || clip.X < 10 || clip.Y < 20 ||
       clip.X+clip.Width > 210 || clip.Y+clip.Height > 120 {
        h.t.Fatal("text outside view")
    }
    h.labels++
}
func (h *toastHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestToast(t *testing.T) {
    h := &toastHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 6; phase++ {
        if App_Frame() != int32(phase) { t.Fatal("toast phase", phase) }
    }
    if h.fills != 4 || h.outlines != 4 || h.labels != 5 {
        t.Fatal("paint count", h.fills, h.outlines, h.labels)
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
