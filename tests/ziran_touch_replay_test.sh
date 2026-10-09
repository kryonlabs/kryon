#!/bin/sh
set -eu

# A touch that begins and ends between two frames is replayed as a press on
# one frame and a release on the next; ordinary touches pass through.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "android_touch"
#import "geometry"
#import "touch_replay"

Same :: (sample: TouchSample, x: float32, y: float32, pressed: bool,
    down: bool, released: bool) -> bool {
    return sample.position.x == x && sample.position.y == y &&
        sample.pressed == pressed && sample.down == down &&
        sample.released == released
}

Idle :: (x: float32, y: float32) -> TouchSample {
    sample: TouchSample
    sample.position = Vector2.{x, y}
    return sample
}

#program_export
Answer :: () -> s32 {
    replay: TouchReplay
    nothing: TouchSpan

    // Ordinary pointer frames pass through unchanged.
    held := Idle(10.0, 20.0)
    held.pressed = true
    held.down = true
    if !Same(TouchReplayStep(*replay, nothing, held), 10.0, 20.0, true, true, false) { return -1 }
    // A touch the platform saw begin is left to the platform.
    began: TouchSpan
    TouchSpanBegin(*began, Vector2.{10.0, 20.0})
    if !Same(TouchReplayStep(*replay, began, held), 10.0, 20.0, true, true, false) { return -2 }

    // A swipe that began and ended within one frame: press where it began,
    // then release where it ended, then nothing.
    swipe: TouchSpan
    TouchSpanBegin(*swipe, Vector2.{50.0, 300.0})
    TouchSpanEnd(*swipe, Vector2.{50.0, 120.0})
    if !Same(TouchReplayStep(*replay, swipe, Idle(50.0, 120.0)), 50.0, 300.0, true, true, false) { return -3 }
    if !Same(TouchReplayStep(*replay, nothing, Idle(50.0, 120.0)), 50.0, 120.0, false, false, true) { return -4 }
    if !Same(TouchReplayStep(*replay, nothing, Idle(50.0, 120.0)), 50.0, 120.0, false, false, false) { return -5 }

    // A release the platform reported itself is not replayed again, but it
    // is reported where the touch lifted rather than where it last moved.
    finished := Idle(50.0, 300.0)
    finished.released = true
    if !Same(TouchReplayStep(*replay, swipe, finished), 50.0, 120.0, false, false, true) { return -6 }
    if !Same(TouchReplayStep(*replay, nothing, finished), 50.0, 300.0, false, false, true) { return -11 }

    // A new touch during a replayed release gets its press on the next frame.
    if !Same(TouchReplayStep(*replay, swipe, Idle(50.0, 120.0)), 50.0, 300.0, true, true, false) { return -7 }
    next := Idle(70.0, 80.0)
    next.pressed = true
    next.down = true
    if !Same(TouchReplayStep(*replay, nothing, next), 50.0, 120.0, false, false, true) { return -8 }
    next.pressed = false
    if !Same(TouchReplayStep(*replay, nothing, next), 70.0, 80.0, true, true, false) { return -9 }
    if !Same(TouchReplayStep(*replay, nothing, next), 70.0, 80.0, false, true, false) { return -10 }

    // raylib's scale from Android coordinates is measured from movements far
    // enough from the press, and kept for axes that barely moved.
    scale := AndroidTouchScale(Vector2.{1.0, 1.0}, Vector2.{100.0, 200.0},
        Vector2.{200.0, 400.0}, Vector2.{100.0, 300.0}, Vector2.{210.0, 600.0})
    if scale.x != 1.0 || scale.y != 0.5 { return -12 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/ir" "$work/app.zi"
for source in source saved; do
    if test "$source" = source; then
        root=$work
        modules="$repo/src/ui --module-path $repo/src/backend"
        app=$work/app.zi
    else
        root=$work/ir
        modules=$work/ir
        app=$work/ir/app.zir
    fi
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$root" --module-path $modules \
        --module-path "$ziran_root/std" \
        --entry app:Answer -o "$work/$source.zib" "$app"
    test "$("$ziran" run "$work/$source.zib")" = 42
done
cmp "$work/source.zib" "$work/saved.zib"

# The Android hook compiles for Android, where it wraps raylib's handler.
cat > "$work/android.zi" <<'ZI'
#import "android_touch"
#import "touch_replay"

#program_export
Sample :: () -> s32 {
    AndroidTouchInstall()
    sample: TouchSample
    return ifx AndroidTouchSample(sample).pressed then 1 else 0
}
ZI
"$ziran" build --target=c --define ANDROID_BUILD --root "$work" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/android" "$work/android.zi"
"${CC:-cc}" -std=c11 -fsyntax-only -I"$ziran_root/include" -I"$work/android" "$work/android"/*.c
echo 'Touch replay: one-frame taps and swipes, platform releases and following touches passed'
