#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" ir --root "$repo/src/ui" -o "$work/ir" \
    "$repo/src/ui/canvas_props.zi"

for input in source saved; do
    if test "$input" = source; then
        root="$repo/src/ui"
        entry="$repo/src/ui/canvas_props.zi"
    else
        root="$work/ir"
        entry="$work/ir/canvas_props.zir"
    fi
    output="$work/$input"
    "$ziran" build --target=c --root "$root" -o "$output" "$entry"
    cat > "$work/main.c" <<'C'
#include "canvas_props.h"

int main(void)
{
    Rectangle hits[3] = {
        {0, 0, 20, 20}, {10, 10, 20, 20}, {100, 100, 10, 10}
    };
    Slice items = {hits, 3};
    Vector2 point = {15, 15};
    if(CanvasHitTest(point, items) != 1) return 1;
    point.x = 80;
    point.y = 80;
    if(CanvasHitTest(point, items) != -1) return 2;
    Canvas canvas = {0};
    canvas.bounds = (Rectangle){10, 20, 100, 100};
    canvas.scroll_x = 5;
    canvas.scroll_y = 7;
    canvas.zoom = 2;
    point = (Vector2){20, 30};
    Vector2 screen = CanvasToScreen(canvas, point);
    if(screen.x != 20 || screen.y != 26) return 3;
    Rectangle rect = CanvasRectToScreen(canvas,
        (Rectangle){20, 30, 4, 6});
    if(rect.x != 20 || rect.y != 26 || rect.width != 8 ||
       rect.height != 12) return 4;
    return 0;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
        "$output"/*.c "$work/main.c" -o "$work/test"
    "$work/test"
done
