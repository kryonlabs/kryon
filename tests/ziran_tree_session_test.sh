#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "drawing_props"
#import "geometry"
#import "layout"
#import "layout_widget"
#import "paint_queue"
#import "session"
#import "tree"
#import "tree_draw"
#import "widget_kind"

using WidgetKind;
using FrameStatus;

#program_export
RasterLine :: (line: Rectangle, color: Color) {}
#program_export
RasterRoundedRectangle :: (bounds: Rectangle, radius: float32,
    segments: s32, color: Color) {}
#program_export
RasterRoundedRectangleOutline :: (bounds: Rectangle, radius: float32,
    segments: s32, width: float32, color: Color) {}
#program_export
RasterText :: (value: string, x: s32, y: s32, font: s32,
    color: Color) {}
#program_export
RasterTextClipped :: (value: string, x: s32, y: s32, font: s32,
    color: Color, clip: Rectangle) {}
#program_export
RasterImage :: (asset_path: string, texture_id: u32,
    source: Rectangle, destination: Rectangle, clip: Rectangle,
    origin: Vector2, rotation: float32, radius: float32,
    tint: Color) {}

Check :: () -> s32 {
    first: Session = SessionOpen()
    second: Session = SessionOpen()
    if !SessionValid(first) || !SessionValid(second) ||
        first.slot == second.slot { return 1 }
    bounds: Rectangle = Rectangle.{0.0, 0.0, 100.0, 60.0}
    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 2 }
    a: s32 = TreeSubmit(first, cast(u64)11, 0, WidgetKindGroup, bounds)
    a_child: s32 = TreeSubmit(first, cast(u64)99, a,
        WidgetKindButton, bounds)
    b: s32 = TreeSubmit(first, cast(u64)12, 0, WidgetKindGroup, bounds)
    b_child: s32 = TreeSubmit(first, cast(u64)99, b,
        WidgetKindButton, bounds)
    if a_child < 0 || b_child < 0 || EndFrame(first) !=
        cast(FrameStatus)FrameOk { return 3 }
    a_generation: u64 = TreeNodeAt(first, a_child).identity_generation
    b_generation: u64 = TreeNodeAt(first, b_child).identity_generation
    if a_generation == b_generation { return 4 }

    if BeginFrame(second, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 5 }
    TreeSubmit(second, cast(u64)99, 0, WidgetKindButton, bounds)
    if EndFrame(second) != cast(FrameStatus)FrameOk ||
        TreeCount(second) != 2 || TreeCount(first) != 5 { return 6 }

    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 7 }
    b = TreeSubmit(first, cast(u64)12, 0, WidgetKindGroup, bounds)
    if TreePrevious(first, b, cast(u64)99, WidgetKindButton) !=
        b_child { return 8 }
    b_child = TreeSubmit(first, cast(u64)99, b, WidgetKindButton, bounds)
    a = TreeSubmit(first, cast(u64)11, 0, WidgetKindGroup, bounds)
    if TreePrevious(first, a, cast(u64)99, WidgetKindButton) !=
        a_child { return 9 }
    a_child = TreeSubmit(first, cast(u64)99, a, WidgetKindButton, bounds)
    if EndFrame(first) != cast(FrameStatus)FrameOk ||
        TreeNodeAt(first, a_child).identity_generation != a_generation ||
        TreeNodeAt(first, b_child).identity_generation != b_generation {
        return 10
    }

    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 18 }
    a = TreeSubmit(first, cast(u64)11, 0, WidgetKindGroup, bounds)
    a_child = TreeSubmit(first, cast(u64)0, a, WidgetKindText, bounds)
    extra: s32 = TreeSubmit(first, cast(u64)0, a,
        WidgetKindText, bounds)
    b = TreeSubmit(first, cast(u64)12, 0, WidgetKindGroup, bounds)
    b_child = TreeSubmit(first, cast(u64)0, b, WidgetKindText, bounds)
    if EndFrame(first) != cast(FrameStatus)FrameOk { return 19 }
    a_generation = TreeNodeAt(first, a_child).identity_generation
    second_auto_generation: u64 =
        TreeNodeAt(first, extra).identity_generation
    b_generation = TreeNodeAt(first, b_child).identity_generation
    if a_generation == b_generation ||
        a_generation == second_auto_generation { return 20 }

    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 21 }
    b = TreeSubmit(first, cast(u64)12, 0, WidgetKindGroup, bounds)
    if TreePrevious(first, b, cast(u64)0, WidgetKindText) != b_child {
        return 22
    }
    b_child = TreeSubmit(first, cast(u64)0, b, WidgetKindText, bounds)
    a = TreeSubmit(first, cast(u64)11, 0, WidgetKindGroup, bounds)
    TreeSubmit(first, cast(u64)33, a, WidgetKindText, bounds)
    if TreePrevious(first, a, cast(u64)0, WidgetKindText) != a_child {
        return 23
    }
    a_child = TreeSubmit(first, cast(u64)0, a, WidgetKindText, bounds)
    if TreePrevious(first, a, cast(u64)0, WidgetKindText) != extra {
        return 24
    }
    extra = TreeSubmit(first, cast(u64)0, a, WidgetKindText, bounds)
    if EndFrame(first) != cast(FrameStatus)FrameOk ||
        TreeNodeAt(first, a_child).identity_generation != a_generation ||
        TreeNodeAt(first, extra).identity_generation !=
            second_auto_generation ||
        TreeNodeAt(first, b_child).identity_generation != b_generation {
        return 25
    }

    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 11 }
    TreeSubmit(first, cast(u64)40, 0, WidgetKindText, bounds)
    if TreeSubmit(first, cast(u64)40, 0, WidgetKindButton, bounds) >= 0 ||
        EndFrame(first) != cast(FrameStatus)FrameDuplicateKey ||
        TreeCount(first) != 7 || PendingPaintCount(first) != 0 {
        return 12
    }
    if BeginFrame(first, cast(u64)7, bounds) !=
        cast(FrameStatus)FrameOk { return 13 }
    a = TreeSubmit(first, cast(u64)11, 0, WidgetKindGroup, bounds)
    if !TreePushScope(first, a) ||
        EndFrame(first) != cast(FrameStatus)FrameInvalidScope ||
        TreeCount(first) != 7 { return 14 }

    if !CloseSession(first) || SessionValid(first) ||
        !SessionValid(second) || TreeCount(second) != 2 { return 15 }
    replacement: Session = SessionOpen()
    if !SessionValid(replacement) || replacement.slot != first.slot ||
        replacement.epoch == first.epoch { return 16 }
    if !CloseSession(replacement) || !CloseSession(second) { return 17 }
    return 42
}

// Native scale check: the portable VM's allocation budget is intentionally
// smaller than a 4,000-node frame, so this checks the generated C path.
Scale :: () -> s32 {
    session: Session = SessionOpen()
    if !SessionValid(session) { return 1 }
    bounds: Rectangle = Rectangle.{0.0, 0.0, 100.0, 60.0}
    repeat: s32 = 0
    last_generation: u64 = cast(u64)0
    while repeat < 2 {
        if BeginFrame(session, cast(u64)7, bounds) !=
            cast(FrameStatus)FrameOk { return 2 }
        row_props: ColumnProps
        row_props.key = cast(u64)80
        row_props.bounds = bounds
        row: LayoutContainer = Row(session, row_props)
        if !row.opened { return 3 }
        child: s32 = -1
        i: s32 = 0
        while i < 4000 {
            child = TreeSubmitCurrent(session, cast(u64)(1000 + i),
                WidgetKindText, Rectangle.{0.0, 0.0, 1.0, 1.0})
            if child < 0 { return 4 }
            i += 1
        }
        if TreeSubmittedBounds(session, child).x != 3999.0 ||
            !End(session) || EndFrame(session) != cast(FrameStatus)FrameOk ||
            TreeCount(session) != 4002 { return 5 }
        generation: u64 = TreeNodeAt(session, child).identity_generation
        if repeat > 0 && generation != last_generation { return 6 }
        last_generation = generation
        repeat += 1
    }
    if !CloseSession(session) { return 7 }
    return 42
}

#program_export
main :: () -> s32 {
    if Check() == 42 && Scale() == 42 { return 0 }
    return 1
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Check -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Check -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
"$ziran" build --target=c --root "$work" \
    --module-path "$repo/src/ui" -o "$work/c" "$work/app.zi"
"${CC:-cc}" -std=c99 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/check"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/check"
