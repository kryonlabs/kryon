#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "cursor"

using CursorShape;

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
    return stage
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 7
test "$("$ziran" run "$work/saved.zib")" = 7
