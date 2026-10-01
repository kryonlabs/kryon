#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
standard=${ZIRAN_STD:-"$ziran_root/std"}
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
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
#import "scroll"
#import "scroll_props"
#import "scroll_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

using WidgetKind;

#program_export
Answer :: () -> s32 {
    root: Rectangle = Rectangle.{0.0, 0.0, 200.0, 100.0}
    TreeStart(TestSession(), cast(u64)1, root)
    props: ScrollProps
    props.key = cast(u64)2
    props.bounds = Rectangle.{20.0, 10.0, 80.0, 40.0}
    props.content_height = 140
    props.scroll_offset = 30
    props.scroll_delta = 20
    result: ScrollResult = Scroll(TestSession(), props)
    if !result.opened || result.node != 1 ||
        result.max_scroll != 100 || result.scroll_offset != 50 ||
        result.viewport.y != 10.0 || result.content.y != -40.0 ||
        result.content.height != 140.0 { return -1 }
    child: Rectangle = Rectangle.{result.content.x - 10.0,
        result.content.y + 45.0, 40.0, 20.0}
    node: s32 = TreeSubmitCurrent(TestSession(), cast(u64)3, WidgetKindButton, child)
    TreeSetInteractive(TestSession(), node, false, false, 3)
    if node != 2 || !End(TestSession()) || !TreeFinish(TestSession()) { return -2 }
    if TreeNodeAt(TestSession(), node).clip.x != 20.0 ||
        TreeNodeAt(TestSession(), node).clip.y != 10.0 ||
        TreeHitAt(TestSession(), 15.0, 15.0) != -1 ||
        TreeHitAt(TestSession(), 25.0, 15.0) != node { return -3 }

    TreeStart(TestSession(), cast(u64)1, root)
    props.scroll_offset = 2147483647
    result = Scroll(TestSession(), props)
    if result.scroll_offset != 100 || !End(TestSession()) ||
        !TreeFinish(TestSession()) { return -4 }
    TreeStart(TestSession(), cast(u64)1, root)
    props.scroll_offset = -2147483647
    props.scroll_delta = -20
    result = Scroll(TestSession(), props)
    if result.scroll_offset != 0 || !End(TestSession()) ||
        !TreeFinish(TestSession()) { return -5 }

    TreeStart(TestSession(), cast(u64)1, root)
    props.scroll_offset = 20
    props.scroll_delta = 0
    props.input.enabled = true
    props.input.pointer_allowed = true
    props.input.wheel = -1.0
    PaintClear(TestSession())
    result = Scroll(TestSession(), props)
    if result.scroll_offset != 62 || !result.frame.consume_wheel ||
        !result.frame.scrollbar || result.frame.clip.width != 70.0 ||
        result.content.y != -52.0 || PendingPaintCount(TestSession()) != 2 ||
        PendingPaintAt(TestSession(), 0).bounds.x != 90.0 ||
        PendingPaintAt(TestSession(), 1).bounds.width <= 0.0 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -6 }

    TreeStart(TestSession(), cast(u64)1, root)
    props.scroll_offset = result.scroll_offset
    props.input.wheel = 0.0
    props.input.pressed = true
    props.input.down = true
    props.input.mouse = Vector2.{95.0, 20.0}
    result = Scroll(TestSession(), props)
    if !result.frame.start_drag || result.frame.clear_drag ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -7 }

    TreeStart(TestSession(), cast(u64)1, root)
    props.input.pressed = false
    props.input = ScrollInputAfter(props.input, result.frame)
    props.input.mouse.y = 45.0
    result = Scroll(TestSession(), props)
    if result.scroll_offset <= 62 || result.scroll_offset > 100 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -8 }

    TreeStart(TestSession(), cast(u64)1, root)
    props.input.down = false
    props.input.released = true
    result = Scroll(TestSession(), props)
    if !result.frame.clear_drag || !result.frame.consume_release ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -9 }
    PaintClear(TestSession())

    // Dragging the content scrolls it once past the threshold, and the
    // press it began with is cancelled so it is not taken as a tap.
    TreeStart(TestSession(), cast(u64)1, root)
    props.scroll_offset = 40
    props.input.released = false
    props.input.owns_drag = false
    props.input.pressed = true
    props.input.down = true
    props.input.mouse = Vector2.{50.0, 40.0}
    result = Scroll(TestSession(), props)
    if !result.frame.start_drag || !result.frame.content_drag ||
        result.frame.drag_moving || result.scroll_offset != 40 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -11 }
    TreeStart(TestSession(), cast(u64)1, root)
    props.input.pressed = false
    props.input = ScrollInputAfter(props.input, result.frame)
    props.input.mouse.y = 42.0
    result = Scroll(TestSession(), props)
    if result.frame.drag_moving || result.scroll_offset != 40 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -12 }
    TreeStart(TestSession(), cast(u64)1, root)
    props.input.mouse.y = 20.0
    result = Scroll(TestSession(), props)
    if !result.frame.drag_moving || !result.frame.cancel_press ||
        result.scroll_offset != 60 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -13 }
    TreeStart(TestSession(), cast(u64)1, root)
    props.input = ScrollInputAfter(props.input, result.frame)
    props.input.down = false
    props.input.released = true
    result = Scroll(TestSession(), props)
    if !result.frame.clear_drag || !result.frame.consume_release ||
        result.frame.content_drag ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -14 }
    props.input.content_drag = false
    props.input.drag_moving = false
    props.input.owns_drag = false
    props.input.released = false
    PaintClear(TestSession())

    TreeStart(TestSession(), cast(u64)1, root)
    props.input.enabled = false
    result = Scroll(TestSession(), props)
    inner: ScrollProps
    inner.key = cast(u64)4
    inner.bounds = Rectangle.{30.0, 15.0, 30.0, 20.0}
    inner.content_height = 50
    unused Scroll(TestSession(), inner)
    if !End(TestSession()) || !End(TestSession()) || !TreeFinish(TestSession()) ||
        TreeScrollAt(TestSession(), 40.0, 20.0) != cast(u64)4 ||
        TreeScrollAt(TestSession(), 25.0, 15.0) != cast(u64)2 ||
        TreeScrollAt(TestSession(), 150.0, 20.0) != cast(u64)0 { return -10 }
    // Horizontal scrolls retain the same clamping and drag policy, but move
    // content on X and reserve a bottom scrollbar instead of a right one.
    return HorizontalScrollTest()
}

HorizontalScrollTest :: () -> s32 {
    root: Rectangle = Rectangle.{0.0, 0.0, 200.0, 100.0}
    TreeStart(TestSession(), cast(u64)1, root)
    sideways: ScrollProps
    sideways.key = cast(u64)8
    sideways.bounds = Rectangle.{20.0, 10.0, 80.0, 40.0}
    sideways.horizontal = true
    sideways.content_width = 180
    sideways.scroll_offset = 20
    sideways.input.enabled = true
    sideways.input.pointer_allowed = true
    sideways.input.wheel = -1.0
    PaintClear(TestSession())
    result: ScrollResult = Scroll(TestSession(), sideways)
    if result.max_scroll != 100 || result.scroll_offset != 62 ||
        result.content.x != -42.0 || result.content.y != 10.0 ||
        result.content.width != 180.0 || result.frame.clip.height != 30.0 ||
        result.frame.paint.track_bounds.y != 40.0 ||
        result.frame.paint.track_bounds.width != 80.0 ||
        PendingPaintCount(TestSession()) != 2 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -15 }

    TreeStart(TestSession(), cast(u64)1, root)
    sideways.input.wheel = 0.0
    sideways.input.pressed = true
    sideways.input.down = true
    sideways.input.mouse = Vector2.{35.0, 45.0}
    result = Scroll(TestSession(), sideways)
    if !result.frame.start_drag || result.frame.content_drag ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -16 }
    TreeStart(TestSession(), cast(u64)1, root)
    sideways.input.pressed = false
    sideways.input.owns_drag = true
    sideways.input.grab = result.frame.grab
    sideways.input.mouse.x = 95.0
    result = Scroll(TestSession(), sideways)
    if result.scroll_offset < 80 || result.scroll_offset > 100 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -17 }
    TreeStart(TestSession(), cast(u64)1, root)
    sideways.input.down = false
    sideways.input.released = true
    result = Scroll(TestSession(), sideways)
    if !result.frame.clear_drag || !result.frame.consume_release ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -18 }

    TreeStart(TestSession(), cast(u64)1, root)
    sideways.input.enabled = false
    sideways.scroll_offset = 2147483647
    result = Scroll(TestSession(), sideways)
    if result.scroll_offset != 100 || result.content.x != -80.0 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -19 }
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

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$standard" \
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
    "$ziran" bundle --root "$root" --module-path "$module_path" --module-path "$standard" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" \
            --root "$root" --module-path "$module_path" --module-path "$standard" \
            -o "$output" "$source"
        if test "$target" = c; then
            cp "$work/native_main.h" "$output/main.c"
            "${CC:-cc}" -std=c11 -I"$include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cp "$work/native_main.h" "$output/main.cpp"
            "${CXX:-c++}" -std=c++17 -I"$include" \
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

# Execute the same widget checks in the portable VM with the maintained
# Ziran no-op raster host, including saved IR; a device is never opened.
cat > "$work/portable.zi" <<'ZI'
Case :: #import "app";
#import "ziran_widget_noop_host"
#program_export
PortableCheck :: () -> s32 { return Case.Answer() }
ZI
set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_widget_noop_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_widget_noop_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_widget_noop_host:RasterLine \
    --bind raster_text:RasterText=ziran_widget_noop_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_widget_noop_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_widget_noop_host:RasterImage
"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/tests" \
    --module-path "$standard" -o "$work/portable-ir" "$work/portable.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$repo/tests" \
    --module-path "$standard" "$@" --entry portable:PortableCheck \
    -o "$work/portable-source.zib" "$work/portable.zi"
"$ziran" bundle --root "$work/portable-ir" --module-path "$work/portable-ir" \
    "$@" --entry portable:PortableCheck -o "$work/portable-saved.zib" "$work/portable-ir/portable.zir"
cmp "$work/portable-source.zib" "$work/portable-saved.zib"
test "$("$ziran" run "$work/portable-source.zib")" = 42
test "$("$ziran" run "$work/portable-saved.zib")" = 42
