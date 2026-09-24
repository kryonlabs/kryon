#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "paint_queue"
#import "scroll"
#import "scroll_props"
#import "scroll_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

Answer :: () -> i32 #export {
    root: Rectangle = (Rectangle){0.0, 0.0, 200.0, 100.0}
    TreeStart((u64)1, root)
    props: ScrollProps
    props.key = (u64)2
    props.bounds = (Rectangle){20.0, 10.0, 80.0, 40.0}
    props.content_height = 140
    props.scroll_offset = 30
    props.scroll_delta = 20
    result: ScrollResult = Scroll(props)
    if !result.opened || result.node != 1 ||
        result.max_scroll != 100 || result.scroll_offset != 50 ||
        result.viewport.y != 10.0 || result.content.y != -40.0 ||
        result.content.height != 140.0 { return -1 }
    child: Rectangle = (Rectangle){result.content.x - 10.0,
        result.content.y + 45.0, 40.0, 20.0}
    node: i32 = TreeSubmitCurrent((u64)3, WidgetKindButton, child)
    TreeSetInteractive(node, false, false, 3)
    if node != 2 || !End() || !TreeFinish() { return -2 }
    if TreeNodeAt(node).clip.x != 20.0 ||
        TreeNodeAt(node).clip.y != 10.0 ||
        TreeHitAt(15.0, 15.0) != -1 ||
        TreeHitAt(25.0, 15.0) != node { return -3 }

    TreeStart((u64)1, root)
    props.scroll_offset = 2147483647
    result = Scroll(props)
    if result.scroll_offset != 100 || !End() ||
        !TreeFinish() { return -4 }
    TreeStart((u64)1, root)
    props.scroll_offset = -2147483647
    props.scroll_delta = -20
    result = Scroll(props)
    if result.scroll_offset != 0 || !End() ||
        !TreeFinish() { return -5 }

    TreeStart((u64)1, root)
    props.scroll_offset = 20
    props.scroll_delta = 0
    props.input.enabled = true
    props.input.pointer_allowed = true
    props.input.wheel = -1.0
    PaintClear()
    result = Scroll(props)
    if result.scroll_offset != 62 || !result.frame.consume_wheel ||
        !result.frame.scrollbar || result.frame.clip.width != 70.0 ||
        result.content.y != -52.0 || PendingPaintCount() != 2 ||
        PendingPaintAt(0).bounds.x != 90.0 ||
        PendingPaintAt(1).bounds.width <= 0.0 ||
        !End() || !TreeFinish() { return -6 }

    TreeStart((u64)1, root)
    props.scroll_offset = result.scroll_offset
    props.input.wheel = 0.0
    props.input.pressed = true
    props.input.down = true
    props.input.mouse = (Vector2){95.0, 20.0}
    result = Scroll(props)
    if !result.frame.start_drag || result.frame.clear_drag ||
        !End() || !TreeFinish() { return -7 }

    TreeStart((u64)1, root)
    props.input.pressed = false
    props.input.owns_drag = true
    props.input.grab = result.frame.grab
    props.input.mouse.y = 45.0
    result = Scroll(props)
    if result.scroll_offset <= 62 || result.scroll_offset > 100 ||
        !End() || !TreeFinish() { return -8 }

    TreeStart((u64)1, root)
    props.input.down = false
    props.input.released = true
    result = Scroll(props)
    if !result.frame.clear_drag || !result.frame.consume_release ||
        !End() || !TreeFinish() { return -9 }
    PaintClear()
    return 42
}
ZI

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#define HOST extern "C"
#else
#include "app.h"
#define HOST
#endif
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
}
HOST void RasterLine(Rectangle bounds, Color color) {
    (void)bounds; (void)color;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/app.zi
        root=$work
        module_path=$repo/src/ui
    else
        source=$work/ir/app.zir
        root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$module_path" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict \
            --root "$root" --module-path "$module_path" \
            -o "$output" "$source"
        if test "$target" = c; then
            cp "$work/native_main.h" "$output/main.c"
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cp "$work/native_main.h" "$output/main.cpp"
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/scroll_test.go" <<'GO'
package ziran
import "testing"
func TestScroll(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("scroll") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
