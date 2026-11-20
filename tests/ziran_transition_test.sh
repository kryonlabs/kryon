#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/transition.zi" "$work/transition.zi"
cp "$repo/src/ui/motion.zi" "$work/motion.zi"
cp "$repo/src/ui/drawing_props.zi" "$work/drawing_props.zi"
cp "$repo/src/ui/geometry.zi" "$work/geometry.zi"
cp "$repo/src/ui/text_align.zi" "$work/text_align.zi"
cp "$ziran_root/std/math.zi" "$work/math.zi"
cat > "$work/use_transition.zi" <<'EOF'
#import "transition"
Motion :: #import "motion";

using TransitionPhase;

Near :: (value: float32, expected: float32) -> bool {
    return value > expected - 0.00002 && value < expected + 0.00002
}

TimedTracksWork :: () -> bool {
    if Motion.Progress(Motion.TimingFor(.Linear), 0.5) != 0.5 ||
        !Near(Motion.Progress(Motion.TimingFor(.Ease), 0.5), 0.8024034) ||
        !Near(Motion.Progress(Motion.TimingFor(.EaseIn), 0.5), 0.3153568) ||
        !Near(Motion.Progress(Motion.TimingFor(.EaseOut), 0.5), 0.6846432) ||
        !Near(Motion.Progress(Motion.TimingFor(.EaseInOut), 0.5), 0.5) { return false }
    if Motion.Progress(Motion.CubicBezier(-1.0, 0.0, 1.0, 1.0), 0.5) != 0.5 ||
        Motion.Progress(Motion.CubicBezier(0.2, 1.4, 0.8, 1.4), 0.5) <= 1.0 ||
        !Near(Motion.Progress(Motion.CubicBezier(1.0, 0.0, 0.0, 1.0), 0.5), 0.5) {
        return false
    }
    frames: [3]Motion.Keyframe = .{
        Motion.Keyframe.{.offset = 0.0, .value = 4.0},
        Motion.Keyframe.{.offset = 0.25, .value = 16.0, .timing = Motion.TimingFor(.Ease)},
        Motion.Keyframe.{.offset = 1.0, .value = 8.0}
    }
    track := Motion.BuildTrack(frames[:])
    if Motion.At(track, 0.125) != 10.0 ||
        Motion.At(track, 0.25) != 16.0 ||
        !Near(Motion.At(track, 0.625), 9.580773) { return false }
    before := Motion.Sample(track, -0.25, 1.0)
    moving := Motion.Sample(track, 0.25, 2.0)
    repeated := Motion.Sample(track, 1.125, 1.0, 2)
    reversed := Motion.Sample(track, 1.375, 1.0, 2, true)
    ended := Motion.Sample(track, 2.0, 1.0, 2, true)
    decimal_end := Motion.Sample(track, 0.6, 0.6)
    reduced := Motion.Sample(track, 0.0, 1.0, 1, false, true)
    instant := Motion.Sample(track, 0.0, 0.0)
    if before.active || before.value != 4.0 || !moving.active || moving.value != 10.0 ||
        !repeated.active || repeated.value != 10.0 || !reversed.active ||
        !Near(reversed.value, 9.580773) || ended.active || ended.value != 4.0 ||
        decimal_end.active || decimal_end.value != 8.0 ||
        reduced.active || reduced.value != 8.0 || instant.active || instant.value != 8.0 {
        return false
    }
    frames[1].offset = 1.0
    invalid := Motion.Sample(Motion.BuildTrack(frames[:]), 0.0, 1.0)
    return !invalid.active && invalid.value == 0.0
}

TweenWorks :: () -> bool {
    state: Motion.TweenState
    timing := Motion.TimingFor(.Linear)
    initial := Motion.TweenTo(*state, 0.0, 50.0, 1.0, timing)
    started := Motion.TweenTo(*state, 10.0, 51.0, 1.0, timing)
    moving := Motion.TweenTo(*state, 10.0, 51.25, 1.0, timing)
    backward := Motion.TweenTo(*state, 10.0, 51.1, 1.0, timing)
    reversal := Motion.TweenTo(*state, 0.0, 51.5, 1.0, timing)
    reversed := Motion.TweenTo(*state, 0.0, 52.0, 1.0, timing)
    settled := Motion.TweenTo(*state, 0.0, 52.5, 1.0, timing)
    reduced := Motion.TweenTo(*state, 6.0, 53.0, 1.0, timing, true)
    immediate := Motion.TweenTo(null, 8.0, 0.0, 1.0, timing)
    return !initial.active && initial.value == 0.0 && started.active && started.value == 0.0 &&
        moving.active && moving.value == 2.5 && backward.active && backward.value == 2.5 &&
        reversal.active && reversal.value == 5.0 && reversed.active && reversed.value == 2.5 &&
        !settled.active && settled.value == 0.0 && !reduced.active && reduced.value == 6.0 &&
        !immediate.active && immediate.value == 8.0
}

#program_export
Answer :: () -> s32 {
    if !TimedTracksWork() { return 9 }
    if !TweenWorks() { return 10 }
    if TransitionClampProgress(-2.0) != 0.0 { return 0 }
    if TransitionClampProgress(2.0) != 1.0 { return 0 }
    if TransitionSmoothProgress(0.5) != 0.5 { return 0 }
    if TransitionDuration(-1.0) <= 0.0 { return 0 }
    if TransitionDelta(-1.0) != 0.0 { return 0 }
    if TransitionReverseElapsed(1.0, 2.0) != 0.0 { return 0 }
    if TransitionAlpha(false, TransitionOut, 0.5, 1.0) != 0.0 { return 0 }
    if TransitionAlpha(true, TransitionOut, 0.5, 1.0) != 0.5 { return 0 }
    if TransitionAlpha(true, TransitionIn, 0.5, 1.0) != 0.5 { return 0 }
    if TransitionAlpha(true, TransitionNone, 0.5, 1.0) != 0.0 { return 0 }
    if TransitionFadeAlphaByte(true, TransitionOut, 0.5, 1.0) != 127 { return 0 }
    if TransitionApplyAlpha(200, 128) != 100 { return 0 }
    if TransitionApplyAlpha(300, 300) != 255 { return 0 }
    state: TransitionState = TransitionStart(1.0)
    if !state.active || state.phase != TransitionOut ||
        TransitionOpacity(state) != 0.0 { return 1 }
    step: TransitionStep = TransitionAdvance(state, 0.5)
    state = step.state
    if step.completed != TransitionNone ||
        TransitionOpacity(state) != 0.5 { return 2 }
    step = TransitionAdvance(state, 0.5)
    state = step.state
    if step.completed != TransitionOut ||
        state.phase != TransitionIn ||
        TransitionOpacity(state) != 1.0 { return 3 }
    step = TransitionAdvance(state, 0.25)
    state = TransitionReverse(step.state)
    if step.completed != TransitionNone ||
        state.phase != TransitionOut ||
        TransitionOpacity(state) != 0.84375 { return 4 }
    step = TransitionAdvance(state, 100.0)
    if step.completed != TransitionOut ||
        !step.state.active { return 5 }
    step = TransitionAdvance(step.state, 100.0)
    if step.completed != TransitionIn ||
        step.state.active ||
        TransitionOpacity(step.state) != 0.0 { return 6 }
    state = TransitionStart(0.0)
    if state.duration_seconds <= 0.0 ||
        state.duration_seconds >= 0.002 { return 7 }
    state = TransitionReset()
    if state.active || state.phase != TransitionNone { return 8 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/transition.zi" \
    "$work/use_transition.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/transition.zi" "$work/use_transition.zi"
"$ziran" bundle --root "$work" --entry use_transition:Answer \
    -o "$work/source.zx" "$work/transition.zi" \
    "$work/use_transition.zi"
"$ziran" bundle --root "$work" --entry use_transition:Answer \
    -o "$work/ir.zx" "$work/ir/transition.zir" \
    "$work/ir/use_transition.zir"
cmp "$work/source.zx" "$work/ir.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/ir.zx")" = 42

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    for target in c go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$input_dir/transition.$extension" \
                "$input_dir/use_transition.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseTransition_Answer() != 42 { panic("wrong transition result") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                -o "$output" "$input_dir/transition.$extension" \
                "$input_dir/use_transition.$extension"
            cat > "$output/main.c" <<'C'
#include "use_transition.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output"/*.c -o "$output/app" -lm
            "$output/app"
        fi
    done
done
