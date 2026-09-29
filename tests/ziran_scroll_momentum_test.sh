#!/bin/sh
set -eu

# Scroll observes the host-sampled scroll device by itself, and a flicked
# content drag keeps coasting, slows down, and stops at a touch or the end.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "geometry"
#import "scroll"
#import "scroll_device"
#import "scroll_props"
#import "scroll_widget"
#import "session"
#import "tree"

test_session: Session;
test_offset: s32;

// One host frame: sample the pointer and clock, then draw one Scroll that
// supplies no input of its own.
TestFrame :: (y: float32, pressed: bool, down: bool, released: bool,
    now: float64, content_height: s32) -> ScrollResult {
    return TestWheelFrame(y, pressed, down, released, 0.0, now, content_height)
}

TestWheelFrame :: (y: float32, pressed: bool, down: bool, released: bool,
    wheel: float32, now: float64, content_height: s32) -> ScrollResult {
    BeginScrollFrame(test_session, Vector2.{50.0, y}, pressed, down, released,
        wheel, now)
    TreeStart(test_session, cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 200.0})
    props: ScrollProps
    props.key = cast(u64)77
    props.scale = 1.0
    props.bounds = Rectangle.{0.0, 0.0, 200.0, 100.0}
    props.content_height = content_height
    props.scroll_offset = test_offset
    result: ScrollResult = Scroll(test_session, props)
    unused End(test_session)
    unused TreeFinish(test_session)
    test_offset = result.scroll_offset
    return result
}

// Presses at y = 90, drags up 20 pixels a frame for three frames, then
// releases on the next frame. Returns the offset at release.
TestFlick :: (start: float64, content_height: s32) -> s32 {
    unused TestFrame(90.0, true, true, false, start, content_height)
    unused TestFrame(70.0, false, true, false, start + 0.016, content_height)
    unused TestFrame(50.0, false, true, false, start + 0.032, content_height)
    unused TestFrame(30.0, false, true, false, start + 0.048, content_height)
    released: ScrollResult = TestFrame(30.0, false, false, true, start + 0.064,
        content_height)
    if !released.frame.flinging { return -1 }
    return released.scroll_offset
}

#program_export
Answer :: () -> s32 {
    test_session = SessionOpen()
    if !SessionValid(test_session) { return -1 }

    // The drag itself moves the content without any caller input.
    unused TestFrame(90.0, true, true, false, 1.0, 2000)
    moved: ScrollResult = TestFrame(40.0, false, true, false, 1.016, 2000)
    if !moved.frame.content_drag || moved.scroll_offset <= 0 { return -2 }
    unused TestFrame(40.0, false, false, true, 1.2, 2000)
    // A drag that rested before release does not coast.
    if ScrollObservationFor(cast(u64)77, Rectangle.{0.0, 0.0, 200.0, 100.0}).flinging {
        return -3
    }

    test_offset = 0
    released: s32 = TestFlick(2.0, 2000)
    if released <= 0 { return -4 }
    // Coasting keeps moving the same way while slowing down.
    now: float64 = 2.064
    previous: s32 = released
    step: s32 = 1 << 30
    frames: s32 = 0
    coasting: bool = true
    while coasting && frames < 400 {
        now += 0.016
        result: ScrollResult = TestFrame(30.0, false, false, false, now, 2000)
        moved_by: s32 = result.scroll_offset - previous
        if moved_by < 0 || moved_by > step { return -5 }
        step = moved_by
        previous = result.scroll_offset
        coasting = result.frame.flinging
        frames += 1
    }
    if coasting || frames < 5 || previous <= released + 50 { return -6 }
    rested: s32 = previous
    now += 0.016
    if TestFrame(30.0, false, false, false, now, 2000).scroll_offset != rested {
        return -7
    }

    // A touch stops a coasting scroll where it is.
    test_offset = 0
    unused TestFlick(10.0, 2000)
    unused TestFrame(30.0, false, false, false, 10.08, 2000)
    touched: ScrollResult = TestFrame(30.0, true, true, false, 10.096, 2000)
    held: s32 = touched.scroll_offset
    if touched.frame.flinging { return -8 }
    if TestFrame(30.0, false, false, true, 10.112, 2000).scroll_offset != held {
        return -9
    }
    if TestFrame(30.0, false, false, false, 10.128, 2000).scroll_offset != held {
        return -10
    }

    // Coasting stops at the end of the content.
    test_offset = 0
    unused TestFlick(20.0, 180)
    now = 20.064
    frames = 0
    last: ScrollResult
    last.frame.flinging = true
    while last.frame.flinging && frames < 400 {
        now += 0.016
        last = TestFrame(30.0, false, false, false, now, 180)
        frames += 1
    }
    if last.frame.flinging || last.scroll_offset != 80 { return -11 }

    // A wheel notch glides to its target instead of jumping there, and a
    // second notch during the glide extends the same target.
    test_offset = 0
    notch: ScrollResult = TestWheelFrame(30.0, false, false, false, -1.0, 30.0, 2000)
    notch_step: s32 = notch.frame.glide_target
    if !notch.frame.gliding || notch_step <= 0 || notch.scroll_offset <= 0 ||
        notch.scroll_offset >= notch_step { return -13 }
    now = 30.0
    previous = notch.scroll_offset
    frames = 0
    glide: ScrollResult = notch
    while glide.frame.gliding && frames < 60 {
        now += 0.016
        glide = TestFrame(30.0, false, false, false, now, 2000)
        if glide.scroll_offset < previous { return -14 }
        previous = glide.scroll_offset
        frames += 1
    }
    if glide.frame.gliding || glide.scroll_offset != notch_step || frames < 3 ||
        frames > 20 { return -15 }
    now += 0.016
    first: ScrollResult = TestWheelFrame(30.0, false, false, false, -1.0, now, 2000)
    now += 0.016
    second: ScrollResult = TestWheelFrame(30.0, false, false, false, -1.0, now, 2000)
    if second.frame.glide_target != notch_step * 3 || first.frame.glide_target != notch_step * 2 {
        return -16
    }
    now += 0.016
    stopped: ScrollResult = TestFrame(30.0, true, true, false, now, 2000)
    if stopped.frame.gliding { return -17 }
    held = stopped.scroll_offset
    now += 0.016
    unused TestFrame(30.0, false, false, true, now, 2000)
    now += 0.016
    if TestFrame(30.0, false, false, false, now, 2000).scroll_offset != held {
        return -18
    }

    if !SessionClose(test_session) { return -12 }
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

# Scroll paints its scrollbar through raster hosts, so run natively with
# stub hosts; the bundle still has to build from source and saved IR.
"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
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
    "$ziran" bundle --root "$root" --module-path "$module_path" --module-path "$repo/../ziran/std" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    output=$work/c-$input
    "$ziran" build --target=c \
        --root "$root" --module-path "$module_path" --module-path "$repo/../ziran/std" \
        -o "$output" "$source"
    cp "$work/native_main.h" "$output/main.c"
    "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
        -I"$output" "$output"/*.c -o "$output/app"
    if ! "$output/app"; then
        echo "scroll momentum ($input) failed" >&2
        exit 1
    fi
done
cmp "$work/source.zib" "$work/saved.zib"
