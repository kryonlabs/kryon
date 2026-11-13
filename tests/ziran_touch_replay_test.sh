#!/bin/sh
set -eu

# A touch that begins and ends between two frames is replayed as a press on
# one frame and a release on the next; ordinary touches pass through.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/touch-replay"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
rm -f "$work/app.zi" "$work/ir/app.zir"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0

cat > "$work/test_case.zi" <<'ZI'
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

    return 42
}
ZI

"$ziran" ir --root "$work" $kryon_project_args --module-path $work \
     -o "$work/ir" "$work/test_case.zi"
for source in source saved; do
    if test "$source" = source; then
        root=$work
        modules="$kryon_project_args --module-path $work"
        app=$work/test_case.zi
    else
        root=$work/ir
        modules="--project --module-path $work/ir"
        app=$work/ir/test_case.zir
    fi
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$root" $modules \
        --entry test_case:Answer -o "$work/$source.zx" "$app"
    test "$("$ziran" run "$work/$source.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"

# The native host receives physical NDK coordinates and scales both axes
# using explicit layout zoom. Exercise its installed callback and immutable
# frame snapshot rather than the removed renderer calibration/hooks.
cat > "$work/android.zi" <<'ZI'
#import "android_touch"
#import "android_native"
#import "android_native_test_host"
#import "android_jni"
#import "touch_replay"
#import "frame_snapshot"
#import "input_protocol"
#import "session"

expected_focus: bool = true;
Send :: (app: *NativeAndroidApp, action: s32, x: float32, y: float32) -> bool {
    event := NativeTestEvent.{.kind = 2, .action = action, .x = x, .y = y}
    return app.input_callback(cast(*void)app, cast(*void)(*event)) == 1
}
Sample :: (session: Session, x: float32, y: float32,
    pressed: bool, down: bool, released: bool) -> bool {
    if !AndroidTouchFrameInputBegin(session, 2.0) { return false }
    pointer := PollPointer()
    repeated := PollPointer()
    published := CaptureFrameInput(session)
    ok := pointer.x == x && pointer.y == y && pointer.pressed == pressed &&
        pointer.down == down && pointer.released == released &&
        repeated.x == x && repeated.y == y && repeated.pressed == pressed &&
        repeated.down == down && repeated.released == released &&
        published.available && published.focused == expected_focus &&
        published.pointer.x == x && published.pointer.y == y &&
        published.pointer.pressed == pressed && published.pointer.down == down &&
        published.pointer.released == released
    FrameInputEnd(session)
    AndroidTouchFrameEnd()
    return ok
}
#program_export
main :: () -> s32 {
    app: NativeAndroidApp
    AndroidNativeStart(*app)
    defer { AndroidNativeClose() }
    if AndroidNativeApp() != *app || app.input_callback == null { return 1 }
    session := SessionOpen()
    defer { unused SessionClose(session) }
    AndroidTouchSetZoom(2.0)
    if !Send(*app, 0, 200.0, 400.0) || !Sample(session, 100.0, 200.0, true, true, false) { return 2 }
    if !Send(*app, 2, 220.0, 600.0) || !Sample(session, 110.0, 300.0, false, true, false) { return 3 }
    if !Send(*app, 1, 260.0, 640.0) || !Sample(session, 130.0, 320.0, false, false, true) { return 4 }
    // A complete tap between frame boundaries still publishes its press
    // and release on separate frames at their respective coordinates.
    if !Send(*app, 0, 20.0, 40.0) || !Send(*app, 1, 60.0, 80.0) ||
        !Sample(session, 10.0, 20.0, true, true, false) { return 5 }
    if !Sample(session, 30.0, 40.0, false, false, true) ||
        !Sample(session, 30.0, 40.0, false, false, false) { return 6 }
    AndroidTouchSetZoom(0.0)
    if !Send(*app, 0, 7.0, 9.0) || !Sample(session, 7.0, 9.0, true, true, false) { return 7 }
    AndroidNativeOnCommand(*app, AndroidNativeCommandLostFocus)
    expected_focus = false
    if !Sample(session, 7.0, 9.0, false, false, true) { return 8 }
    AndroidNativeOnCommand(*app, AndroidNativeCommandGainedFocus)
    expected_focus = true
    if !Send(*app, 0, 18.0, 22.0) || !Sample(session, 18.0, 22.0, true, true, false) { return 9 }
    if native_test_unexpected_effects != 0 || native_test_finish_calls != 0 { return 10 }
    return 0
}
ZI

"$ziran" ir --define ANDROID_BUILD --define __x86_64__ --root "$work" \
     $kryon_project_args --module-path $work \
    -o "$work/native-ir" "$work/android.zi"
for route in source saved; do
    input="$work/android.zi"
    root="$work"
    if [ "$route" = saved ]; then
        input="$work/native-ir/android.zir"
        root="$work/native-ir"
    fi
    output="$work/native-$route"
    "$ziran" build --target=c --prune-stale --define ANDROID_BUILD --define __x86_64__ \
        --no-main --entry android:main --root "$root" \
         $kryon_project_args --module-path $work \
        -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
        -I"$ziran_root/include" $includes $(find "$output" -name '*.c') \
        "$ziran_root/build/libziran.a" -Wl,--gc-sections -lm -lpthread -ldl -o "$work/test"
    timeout 30s "$work/test"
done
printf '%s\n' 'Touch replay: portable quick taps, swipes and releases; NativeActivity zoom, immutable frames and focus passed (source and saved IR)'
