#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cat > "$work/app.zi" <<'ZI'
#import "cursor"
#import "geometry"
#import "session"
#import "tree"
#import "tree_input"
#import "widget_kind"

using CursorShape;

TreeCursorTest :: () -> s32 {
    session := SessionOpen()
    if !SessionValid(session) { return -20 }
    defer SessionClose(session)
    defer TreeInputReset(session)
    defer TreeDispose(session)
    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    button := TreeSubmit(session, cast(u64)2, 0, cast(s32)WidgetKind.WidgetKindButton, .{10.0, 10.0, 40.0, 30.0})
    TreeSetInteractive(session, button, false, false, 2)
    field := TreeSubmit(session, cast(u64)3, 0, cast(s32)WidgetKind.WidgetKindTextField, .{70.0, 10.0, 40.0, 30.0})
    TreeSetInteractive(session, field, false, false, 3)
    disabled := TreeSubmit(session, cast(u64)4, 0, cast(s32)WidgetKind.WidgetKindButton, .{130.0, 10.0, 40.0, 30.0})
    TreeSetInteractive(session, disabled, true, false, 4)
    scroll := TreeSubmit(session, cast(u64)5, 0, cast(s32)WidgetKind.WidgetKindScroll, .{10.0, 60.0, 80.0, 30.0})
    TreeSetChildClip(session, scroll, .{10.0, 60.0, 30.0, 30.0})
    clipped := TreeSubmit(session, cast(u64)6, scroll, cast(s32)WidgetKind.WidgetKindButton, .{10.0, 60.0, 80.0, 30.0})
    TreeSetInteractive(session, clipped, false, false, 6)
    if !TreeFinish(session) { return -21 }
    positions: [7]Vector2 = .{.{30.0, 20.0}, .{90.0, 20.0}, .{150.0, 20.0}, .{55.0, 20.0}, .{20.0, 70.0}, .{60.0, 70.0}, .{-1.0, 20.0}}
    shapes: [7]s32 = .{cast(s32)CursorClickable, cast(s32)CursorText, cast(s32)CursorDisabled, cast(s32)CursorDefault, cast(s32)CursorClickable, cast(s32)CursorDefault, cast(s32)CursorDefault}
    for index: 0..6 {
        point := positions[index]
        TreePointerUpdate(session, PointerFrame.{point.x, point.y, false, false, false})
        if TreeCursor(session) != shapes[index] { return -22 }
    }
    if TreeHitAt(session, 150.0, 20.0) != -1 { return -30 }
    TreeStart(session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    button = TreeSubmit(session, cast(u64)2, 0, cast(s32)WidgetKind.WidgetKindButton, .{10.0, 10.0, 40.0, 30.0})
    TreeSetInteractive(session, button, false, false, 2)
    modal := TreeSubmit(session, cast(u64)7, 0, cast(s32)WidgetKind.WidgetKindModal, .{0.0, 0.0, 200.0, 100.0})
    field = TreeSubmit(session, cast(u64)8, modal, cast(s32)WidgetKind.WidgetKindTextField, .{70.0, 10.0, 40.0, 30.0})
    TreeSetInteractive(session, field, false, false, 8)
    if !TreeFinish(session) { return -31 }
    TreePointerUpdate(session, PointerFrame.{30.0, 20.0, false, false, false})
    if TreeCursor(session) != cast(s32)CursorDefault { return -32 }
    TreePointerUpdate(session, PointerFrame.{90.0, 20.0, false, false, false})
    if TreeCursor(session) != cast(s32)CursorText { return -33 }
    if TreeCursor(Session.{}) != cast(s32)CursorDefault { return -34 }
    return 42
}

#program_export
Answer :: () -> s32 {
    state: CursorFrame
    stage: s32 = 0
    while stage < 7 {
        state = CursorBeginFrame(state)
        if stage == 0 {
            state = CursorRequest(state, cast(s32)CursorClickable)
            state = CursorRequest(state, cast(s32)CursorDisabled)
        } else if stage == 1 {
            state = CursorRequest(state, cast(s32)CursorText)
            state = CursorRequest(state, cast(s32)CursorResizeHorizontal)
        } else if stage == 4 {
            state = CursorRequest(state, cast(s32)CursorText)
            state = CursorRequest(state, cast(s32)CursorClickable)
        } else if stage == 5 {
            state = CursorRequest(state, cast(s32)CursorClickable)
            state = CursorRequest(state, cast(s32)CursorCrosshair)
        } else if stage == 6 {
            state = CursorRequest(state, 99)
        }
        decision: CursorDecision = CursorEndFrame(state)
        if stage == 0 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorClickable) { return -1 }
        if stage == 1 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorResizeHorizontal) { return -2 }
        if stage == 2 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorDefault) { return -3 }
        if stage == 3 && decision.apply { return -4 }
        if stage == 4 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorClickable) { return -5 }
        if stage == 5 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorCrosshair) { return -6 }
        if stage == 6 && (!decision.apply ||
            decision.state.applied != cast(s32)CursorDefault) { return -7 }
        state = decision.state
        stage += 1
    }
    tested := TreeCursorTest()
    if tested != 42 { return tested }
    return stage
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry app:Answer -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 7
test "$("$ziran" run "$work/saved.zx")" = 7
