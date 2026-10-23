#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cat > "$work/app.zi" <<'ZI'
#import "geometry"
#import "scroll"
#import "scroll_device"
#import "scroll_props"
#import "session"
#import "tree"
#import "tree_input"
#import "widget_kind"
using WidgetKind;

#program_export
Answer :: () -> s32 {
    session: Session = SessionOpen()
    if !SessionValid(session) { return -8 }
    bounds: Rectangle = Rectangle.{20.0, 10.0, 80.0, 40.0}
    BeginScrollFrame(session, Vector2.{95.0, 20.0}, true, true, false, -1.0, 0.0)
    first: ScrollInput = ScrollObservationFor(cast(u64)7, bounds)
    if !first.enabled || !first.device || !first.pointer_allowed ||
        first.owns_drag || first.owner_captured || first.wheel != -1.0 { return -1 }

    frame: ScrollFrame
    frame.start_drag = true
    frame.grab = 3.0
    frame.consume_wheel = true
    CommitScroll(cast(u64)7, frame)
    other: ScrollInput = ScrollObservationFor(cast(u64)8, bounds)
    if other.pointer_allowed || !other.owner_captured ||
        other.wheel != 0.0 { return -2 }
    own: ScrollInput = ScrollObservationFor(cast(u64)7, bounds)
    if !own.owns_drag || own.grab != 3.0 || own.wheel != 0.0 {
        return -3
    }

    BeginScrollFrame(session, Vector2.{150.0, 70.0}, false, true, false, 0.0, 0.0)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if !own.owns_drag || !own.pointer_allowed || own.grab != 3.0 {
        return -4
    }
    BeginScrollFrame(session, Vector2.{150.0, 70.0}, false, false, true, 0.0, 0.0)
    frame.start_drag = false
    frame.clear_drag = true
    CommitScroll(cast(u64)7, frame)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if own.owns_drag || own.grab != 0.0 { return -5 }

    // A content drag keeps its origin across frames and, once it moves,
    // cancels the press on the node beneath it.
    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    button: s32 = TreeSubmit(session, cast(u64)9, 0, WidgetKindButton, bounds)
    TreeSetInteractive(session, button, false, false, 0)
    if !TreeFinish(session) { return -10 }
    unused TreePointerUpdate(session, PointerFrame.{50.0, 30.0, true, true, false})
    BeginScrollFrame(session, Vector2.{50.0, 30.0}, true, true, false, 0.0, 0.0)
    drag: ScrollFrame
    drag.start_drag = true
    drag.content_drag = true
    drag.drag_origin_y = 30.0
    drag.drag_origin_offset = 12
    CommitScroll(cast(u64)7, drag)
    BeginScrollFrame(session, Vector2.{50.0, 10.0}, false, true, false, 0.0, 0.0)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if !own.owns_drag || !own.content_drag || own.drag_origin_y != 30.0 ||
        own.drag_origin_offset != 12 || own.drag_moving { return -11 }
    drag.start_drag = false
    drag.drag_moving = true
    drag.cancel_press = true
    CommitScroll(cast(u64)7, drag)
    unused TreePointerUpdate(session, PointerFrame.{50.0, 30.0, false, false, true})
    if TreeTakeActivationAt(session, button) { return -12 }
    BeginScrollFrame(session, Vector2.{50.0, 10.0}, false, false, false, 0.0, 0.0)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if own.owns_drag || own.content_drag { return -13 }

    // Drag controls reserve their press; the surrounding scroll still gets
    // wheel input, and cancellation makes ordinary content scrolling usable.
    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    button = TreeSubmit(session, cast(u64)9, 0, WidgetKindButton, bounds)
    TreeSetInteractive(session, button, false, false, 0)
    TreeSetDragCapture(session, button, true)
    if !TreeFinish(session) { return -14 }
    unused TreePointerUpdate(session, PointerFrame.{50.0, 30.0, true, true, false})
    BeginScrollFrame(session, Vector2.{50.0, 30.0}, true, true, false, -1.0, 0.0)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if own.pointer_allowed || own.owns_drag || own.wheel != -1.0 ||
        !TreePointerCapturesDrag(session) { return -15 }
    unused TreePointerUpdate(session, PointerFrame.{150.0, 70.0, true, false, false})
    BeginScrollFrame(session, Vector2.{150.0, 70.0}, false, true, false, 0.0, 0.0)
    if !TreePointerCapturesDrag(session) { return -16 }
    TreePointerCancel(session)
    BeginScrollFrame(session, Vector2.{50.0, 30.0}, true, true, false, 0.0, 0.0)
    own = ScrollObservationFor(cast(u64)7, bounds)
    if !own.pointer_allowed || TreePointerCapturesDrag(session) { return -17 }

    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    outer: s32 = TreeSubmit(session, cast(u64)7, 0, WidgetKindScroll, bounds)
    inner_bounds: Rectangle = Rectangle.{30.0, 15.0, 30.0, 20.0}
    unused TreeSubmit(session, cast(u64)8, outer, WidgetKindScroll, inner_bounds)
    if !TreeFinish(session) { return -6 }
    BeginScrollFrame(session, Vector2.{40.0, 20.0}, false, false, false, -1.0, 0.0)
    first = ScrollObservationFor(cast(u64)7, bounds)
    other = ScrollObservationFor(cast(u64)8, inner_bounds)
    if first.pointer_allowed || first.wheel != 0.0 ||
        !other.pointer_allowed || other.wheel != -1.0 { return -7 }

    // A sideways wheel moves a sideways scroll, toward its end when it moves
    // right, and leaves an upright one alone. A sideways scroll moves with
    // the upright wheel too.
    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    unused TreeSubmit(session, cast(u64)7, 0, WidgetKindScroll, bounds)
    if !TreeFinish(session) { return -18 }
    BeginScrollFrame(session, Vector2.{40.0, 20.0}, false, false, false, 0.0, 0.0)
    ScrollDeviceSideways(session, 1.0)
    upright: ScrollInput = ScrollObservationFor(cast(u64)7, bounds)
    sideways: ScrollInput = ScrollObservationFor(cast(u64)7, bounds, true)
    if upright.wheel != 0.0 || sideways.wheel != -1.0 { return -19 }
    BeginScrollFrame(session, Vector2.{40.0, 20.0}, false, false, false, -1.0, 0.0)
    sideways = ScrollObservationFor(cast(u64)7, bounds, true)
    upright = ScrollObservationFor(cast(u64)7, bounds)
    if sideways.wheel != -1.0 || upright.wheel != -1.0 { return -20 }
    if !SessionClose(session) { return -9 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
for source in source saved; do
    if test "$source" = source; then
        root=$work
        modules=$repo/src/ui
        app=$work/app.zi
    else
        root=$work/ir
        modules=$work/ir
        app=$work/ir/app.zir
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" \
        --module-path "$ziran_root/std" \
        --entry app:Answer -o "$work/$source.zx" "$app"
    test "$("$ziran" run "$work/$source.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"
