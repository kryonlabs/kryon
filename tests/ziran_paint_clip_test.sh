#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

#import "drawing_props"
#import "geometry"
#import "paint_queue"
#import "tree"
#import "widget_kind"

using WidgetKind;

#program_export
Frame :: () -> s32 {
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    viewport: s32 = TreeSubmit(TestSession(), cast(u64)2, 0, WidgetKindCard,
        Rectangle.{0.0, 0.0, 90.0, 90.0})
    TreeSetChildClip(TestSession(), viewport, Rectangle.{20.0, 20.0, 40.0, 40.0})
    child: s32 = TreeSubmit(TestSession(), cast(u64)3, viewport, WidgetKindText,
        Rectangle.{10.0, 10.0, 70.0, 70.0})
    ink: Color = ColorFromPacked(cast(u32)0x171717ff)
    PaintLabel(TestSession(), child, "Clip", 10, 10, 16, ink)
    PaintImage(TestSession(), child, "", cast(u32)7,
        Rectangle.{0.0, 0.0, 70.0, 70.0},
        Rectangle.{10.0, 10.0, 70.0, 70.0},
        Rectangle.{10.0, 10.0, 70.0, 70.0},
        Vector2.{0.0, 0.0}, 0.0, 0.0, ink)
    if !TreeFinish(TestSession()) { return -1 }
    PaintFlush(TestSession())
    return 42
}
ZI

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

static int text_draws;
static int image_draws;

HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color; assert(0);
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width;
    (void)color; assert(0);
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
    assert(value.length == 4 && value.data[0] == 'C' &&
           x == 10 && y == 10 && font == 16);
    assert(color.r == 0x17 && color.a == 255);
    assert(clip.x == 20 && clip.y == 20 &&
           clip.width == 40 && clip.height == 40);
    text_draws++;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)source; (void)destination; (void)origin;
    (void)rotation; (void)radius;
    assert(path.length == 0 && id == 7 && tint.a == 255);
    assert(clip.x == 20 && clip.y == 20 &&
           clip.width == 40 && clip.height == 40);
    image_draws++;
}
int main(void) {
    assert(Frame() == 42);
    assert(text_draws == 1 && image_draws == 1);
    return 0;
}
C

for target in c cpp go; do
    output=$work/$target
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
        cat > "$output/paint_clip_test.go" <<'GO'
package ziran
import "testing"

type clipHost struct { t *testing.T; texts int; images int }

func (h *clipHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) { h.t.Fatal("unexpected fill") }
func (h *clipHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32,
    color Color) { h.t.Fatal("unexpected outline") }
func (h *clipHost) RasterLine(bounds Rectangle,
    color Color) { h.t.Fatal("unexpected line") }
func (h *clipHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected plain text") }
func (h *clipHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Clip" || x != 10 || y != 10 || font != 16 ||
       clip.X != 20 || clip.Y != 20 ||
       clip.Width != 40 || clip.Height != 40 ||
       color.R != 0x17 || color.A != 255 { h.t.Fatal("text clip") }
    h.texts++
}
func (h *clipHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    if path != "" || id != 7 ||
       clip.X != 20 || clip.Y != 20 ||
       clip.Width != 40 || clip.Height != 40 ||
       tint.A != 255 { h.t.Fatal("image clip") }
    h.images++
}
func TestClip(t *testing.T) {
    h := &clipHost{t: t}
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    if App_Frame() != 42 || h.texts != 1 || h.images != 1 {
        t.Fatal("clip paint calls", h.texts, h.images)
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
