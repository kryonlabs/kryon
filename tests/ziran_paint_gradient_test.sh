#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_dir=$(CDPATH= cd -- "$(dirname -- "$ziran")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "drawing_props"
#import "geometry"
#import "paint_queue"
#import "tree"

test_session: Session;

#program_export
Answer :: () -> s32 {
    test_session = SessionOpen()
    root: Rectangle = Rectangle.{0.0, 0.0, 100.0, 100.0}
    top: Color = Color.{0, 0, 0, 0}
    bottom: Color = Color.{200, 100, 50, 255}
    middle: Color = GradientColorAt(top, bottom, 0.5)
    if middle.r != 100 || middle.g != 50 || middle.b != 25 ||
        middle.a != 128 { return -1 }
    if GradientColorAt(top, bottom, 2.0).a != 255 ||
        GradientColorAt(top, bottom, -1.0).a != 0 { return -2 }

    // Inside a frame the gradient is queued behind earlier paint, so it
    // lands on top of an image submitted before it.
    TreeStart(test_session, cast(u64)1, root)
    PaintVerticalGradient(test_session, -1,
        Rectangle.{10.0, 20.0, 30.0, 4.0}, top, bottom)
    if PendingPaintCount(test_session) != 1 ||
        PendingPaintAt(test_session, 0).kind != 7 ||
        PendingPaintAt(test_session, 0).end_color.a != 255 { return -3 }
    if !TreeFinish(test_session) { return -4 }
    PaintFlush(test_session)
    return 42
}
ZI

cat > "$work/main.c" <<'C'
#include "app.h"
static int rows;
static float last_y, last_height;
static Color first, last;
void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)radius; (void)segments;
    if (rows == 0) first = color;
    last = color;
    last_y = bounds.y;
    last_height = bounds.height;
    rows++;
}
void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
}
void RasterLine(Rectangle bounds, Color color) { (void)bounds; (void)color; }
void RasterText(String value, int32_t x, int32_t y, int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
}
void RasterTextClipped(String value, int32_t x, int32_t y, int32_t font,
    Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
}
void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
}
int main(void) {
    if (Answer() != 42) return 1;
    /* Four one-pixel rows sampled at 1/8, 3/8, 5/8, and 7/8. */
    if (rows != 4 || last_y != 23.0f || last_height != 1.0f) return 2;
    if (first.a != 32 || last.a != 223 || last.r != 175) return 3;
    return 0;
}
C

"$ziran" build --target=c --root "$work" --module-path "$repo/src/ui" \
    --module-path "$ziran_dir/std" -o "$work/c" "$work/app.zi"
cp "$work/main.c" "$work/c/main.c"
"${CC:-cc}" -std=c11 -I"$ziran_dir/include" -I"$work/c" "$work/c"/*.c \
    -lm -o "$work/app"
"$work/app"
echo "Kryon vertical gradient paint passed"
