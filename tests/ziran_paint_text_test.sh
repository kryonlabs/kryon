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
label_bytes: [8]u8;

// A caller formats a label into its own buffer, queues it, and reuses the
// buffer before the frame is flushed. The queued label keeps its text.
QueueLabelFromBuffer :: () {
    label_bytes[0] = cast(u8)79
    label_bytes[1] = cast(u8)75
    PaintLabel(test_session, -1, TextView(label_bytes[0:2]), 4, 6, 14,
        Color.{1, 2, 3, 255})
    label_bytes[0] = cast(u8)88
    label_bytes[1] = cast(u8)88
}

#program_export
Answer :: () -> s32 {
    test_session = SessionOpen()
    root: Rectangle = Rectangle.{0.0, 0.0, 100.0, 100.0}
    TreeStart(test_session, cast(u64)1, root)
    QueueLabelFromBuffer()
    if PendingPaintCount(test_session) != 1 ||
        PendingPaintAt(test_session, 0).value != "OK" { return -1 }
    if !TreeFinish(test_session) { return -2 }
    PaintFlush(test_session)
    return 42
}
ZI

cat > "$work/main.c" <<'C'
#include <string.h>
#include "app.h"
static int labels;
static int matched;
void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
}
void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
}
void RasterLine(Rectangle bounds, Color color) { (void)bounds; (void)color; }
void RasterText(String value, int32_t x, int32_t y, int32_t font, Color color) {
    (void)x; (void)y; (void)font; (void)color;
    labels++;
    if (value.length == 2 && memcmp(value.data, "OK", 2) == 0) matched++;
}
void RasterTextClipped(String value, int32_t x, int32_t y, int32_t font,
    Color color, Rectangle clip) {
    (void)clip;
    RasterText(value, x, y, font, color);
}
void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
}
int main(void) {
    if (Answer() != 42) return 1;
    if (labels != 1 || matched != 1) return 2;
    return 0;
}
C

"$ziran" build --target=c --root "$work" --module-path "$repo/src/ui" \
    --module-path "$ziran_dir/std" -o "$work/c" "$work/app.zi"
cp "$work/main.c" "$work/c/main.c"
"${CC:-cc}" -std=c11 -I"$ziran_dir/include" -I"$work/c" "$work/c"/*.c \
    -lm -o "$work/app"
"$work/app"
echo "Kryon queued labels keep their text"
