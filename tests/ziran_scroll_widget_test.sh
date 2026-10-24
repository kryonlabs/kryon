#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
standard=${ZIRAN_STD:-"$ziran_root/std"}
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/tests/widget_test_session.zi" "$work/"

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "widget_test_session"

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
    metrics: ScrollMetrics
    metrics.scrollbar_width = 10
    metrics.thumb_min_height = 16
    metrics.thumb_inset = 2
    half := ScrollBarPaintFor(90, 10, 100, 200, 0, 100, metrics)
    bottom := ScrollBarPaintFor(90, 10, 100, 200, 100, 100, metrics)
    full := ScrollBarPaintFor(90, 10, 100, 100, 0, 0, metrics)
    tiny := ScrollBarPaintFor(90, 10, 2, 1000, 998, 998, metrics)
    if half.thumb_bounds.height != 50.0 || half.track_span != 50 ||
        bottom.thumb_bounds.y + bottom.thumb_bounds.height != 110.0 ||
        full.thumb_bounds.height != 100.0 || full.track_span != 0 ||
        tiny.thumb_bounds.height != 2.0 || tiny.thumb_bounds.y != 10.0 {
        return -20
    }
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
    horizontal: s32 = HorizontalScrollTest()
    if horizontal != 42 { return horizontal }
    return KeptScrollbarTest()
}

// A kept scrollbar shows, and narrows the content, while the content still
// fits, so content that grows keeps its width; with nothing to scroll it
// takes no press.
KeptScrollbarTest :: () -> s32 {
    root: Rectangle = Rectangle.{0.0, 0.0, 200.0, 100.0}
    TreeStart(TestSession(), cast(u64)1, root)
    kept: ScrollProps
    kept.key = cast(u64)9
    kept.bounds = Rectangle.{20.0, 10.0, 80.0, 40.0}
    kept.content_height = 30
    kept.keep_scrollbar = true
    kept.input.enabled = true
    kept.input.pointer_allowed = true
    PaintClear(TestSession())
    result: ScrollResult = Scroll(TestSession(), kept)
    if result.max_scroll != 0 || result.scroll_offset != 0 ||
        !result.frame.scrollbar || result.frame.clip.width != 70.0 ||
        result.content.width != 70.0 ||
        result.frame.paint.thumb_bounds.height != 40.0 ||
        PendingPaintCount(TestSession()) != 2 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -21 }

    // Neither the bar nor the content starts a drag, and the thumb does not
    // light up under the pointer.
    TreeStart(TestSession(), cast(u64)1, root)
    kept.input.pressed = true
    kept.input.down = true
    kept.input.mouse = Vector2.{95.0, 20.0}
    result = Scroll(TestSession(), kept)
    if result.frame.start_drag || result.frame.content_drag ||
        result.frame.thumb_state != 1 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -22 }
    TreeStart(TestSession(), cast(u64)1, root)
    kept.input.mouse = Vector2.{50.0, 20.0}
    result = Scroll(TestSession(), kept)
    if result.frame.start_drag || result.frame.content_drag ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -23 }

    // Once the content grows past the view, the same bar scrolls it, and
    // the content keeps the width it had.
    TreeStart(TestSession(), cast(u64)1, root)
    kept.content_height = 140
    kept.input.mouse = Vector2.{95.0, 20.0}
    result = Scroll(TestSession(), kept)
    if result.max_scroll != 100 || !result.frame.start_drag ||
        result.frame.clip.width != 70.0 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -24 }

    // Without it, content that fits has no scrollbar and its full width.
    TreeStart(TestSession(), cast(u64)1, root)
    kept.content_height = 30
    kept.keep_scrollbar = false
    kept.input.pressed = false
    kept.input.down = false
    PaintClear(TestSession())
    result = Scroll(TestSession(), kept)
    if result.frame.scrollbar || result.frame.clip.width != 80.0 ||
        PendingPaintCount(TestSession()) != 0 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -25 }
    PaintClear(TestSession())
    if FocusScrollTest() != 42 { return -30 }
    return 42
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
// The same selected input survives keyboard resizing and manual scrolling.
FocusScrollTest :: () -> s32 {
    session := SessionOpen()
    viewport := Rectangle.{0.0, 0.0, 240.0, 300.0}
    offset: s32 = 0
    for step: cast(s32)0..5 {
        if step == 1 { viewport.height = 160.0 }
        if step == 2 { offset = 300 }
        TreeStart(session, cast(u64)900, Rectangle.{0.0, 0.0, 240.0, 700.0})
        props := ScrollProps.{.key = cast(u64)901, .bounds = viewport,
            .content_height = 1000, .scroll_offset = offset,
            .input = ScrollInput.{.enabled = true}}
        result := Scroll(session, props)
        if step == 1 && result.scroll_offset != 50 { return -1 }
        if (step == 2 || step == 3) && result.scroll_offset != 300 { return -2 }
        if step == 4 && result.scroll_offset != 600 { return -3 }
        if step == 5 && result.scroll_offset != 600 { return -4 }
        offset = result.scroll_offset
        first := TreeSubmitCurrent(session, cast(u64)902, WidgetKindTextField,
            Rectangle.{16.0, 150.0 - cast(float32)offset, 180.0, 44.0})
        TreeSetInteractive(session, first, false, false, 902)
        TreeSetSelected(session, first, step < 3)
        second := TreeSubmitCurrent(session, cast(u64)903, WidgetKindTextField,
            Rectangle.{16.0, 700.0 - cast(float32)offset, 180.0, 44.0})
        TreeSetInteractive(session, second, false, false, 903)
        TreeSetSelected(session, second, step >= 3 && step < 4)
        if !End(session) { return -5 }
        outside := TreeSubmitCurrent(session, cast(u64)904, WidgetKindTextField,
            Rectangle.{16.0, 600.0, 180.0, 44.0})
        TreeSetInteractive(session, outside, false, false, 904)
        TreeSetSelected(session, outside, step >= 4)
        if !TreeFinish(session) { return -6 }
        PaintClear(session)
    }
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
HOST void RasterImageMesh(String path, uint32_t id, Rectangle source,
    Slice points, int32_t columns, int32_t rows, Rectangle clip, Color tint) {
    (void)path; (void)id; (void)source; (void)points;
    (void)columns; (void)rows; (void)clip; (void)tint;
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
        --entry app:Answer -o "$work/$input.zx" "$source"
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
cmp "$work/source.zx" "$work/saved.zx"

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
    --bind paint_queue:RasterImage=ziran_widget_noop_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh
"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/tests" \
    --module-path "$standard" -o "$work/portable-ir" "$work/portable.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$repo/tests" \
    --module-path "$standard" "$@" --entry portable:PortableCheck \
    -o "$work/portable-source.zx" "$work/portable.zi"
"$ziran" bundle --root "$work/portable-ir" --module-path "$work/portable-ir" \
    "$@" --entry portable:PortableCheck -o "$work/portable-saved.zx" "$work/portable-ir/portable.zir"
cmp "$work/portable-source.zx" "$work/portable-saved.zx"
test "$("$ziran" run "$work/portable-source.zx")" = 42
test "$("$ziran" run "$work/portable-saved.zx")" = 42
