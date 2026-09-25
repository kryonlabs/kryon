#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

#import "control_props"
#import "geometry"
#import "modal_props"
#import "modal_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase: s32;

FindLabel :: (label: string) -> s32 {
    index: s32 = 0
    while index < TreeCount(TestSession()) {
        if TreeNodeAt(TestSession(), index).semantic_label == label {
            return index
        }
        index += 1
    }
    return -1
}

Click :: (index: s32) -> bool {
    if index < 0 { return false }
    bounds: Rectangle = TreeNodeAt(TestSession(), index).bounds
    x: float32 = bounds.x + bounds.width * 0.5
    y: float32 = bounds.y + bounds.height * 0.5
    TreePointerUpdate(TestSession(), PointerFrame.{x, y, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{x, y, false, false, true})
    return true
}

#program_export
Frame :: () -> s32 {
    actions: [3]ActionModalAction
    actions[0].label = "Cancel"
    actions[0].tone = cast(ButtonTone)ButtonToneNeutral
    actions[0].emphasis = cast(ButtonEmphasis)ButtonEmphasisSoft
    actions[1].label = "Delete"
    actions[1].tone = cast(ButtonTone)ButtonToneAccent
    actions[1].emphasis = cast(ButtonEmphasis)ButtonEmphasisFilled
    actions[2].label = "Details"
    actions[2].disabled = true
    props: ActionModalProps
    props.key = cast(u64)42
    props.title = "Confirm"
    props.message = "Delete the selected file and every associated version?"
    props.show_close = true
    props.close_label = "Close"
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 300.0, 280.0})
    result: ActionModalResult = ActionModal(TestSession(), props, actions[0:3])
    if !TreeFinish(TestSession()) || result.node != 1 { return -20 }
    if phase != 3 && phase != 5 && (
        FindLabel("Delete the selected file and every associated version?") < 0 ||
        TreeNodeAt(TestSession(), FindLabel("Delete the selected file and every associated version?")).bounds.height <= 14.0 ||
        FindLabel("Cancel") < 0 ||
        FindLabel("Delete") < 0 ||
        FindLabel("Details") < 0 ||
        FindLabel("Close") < 0 ||
        TreeNodeAt(TestSession(), FindLabel("Details")).bounds.y <=
            TreeNodeAt(TestSession(), FindLabel("Cancel")).bounds.y) {
        return -20
    }
    if phase == 0 {
        if result.action != 0 ||
            !Click(FindLabel("Delete")) { return -1 }
    } else if phase == 1 {
        if result.action != 2 ||
            !Click(FindLabel("Details")) { return -2 }
    } else if phase == 2 {
        if result.action != 0 { return -3 }
        TreePointerUpdate(TestSession(), PointerFrame.{5.0, 5.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{5.0, 5.0,
            false, false, true})
    } else if phase == 3 {
        if result.action != -1 { return -4 }
    } else if phase == 4 {
        if result.action != 0 ||
            !Click(FindLabel("Close")) { return -5 }
    } else if phase == 5 {
        if result.action != -1 { return -6 }
    }
    old: s32 = phase
    phase += 1
    return old
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Frame -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Frame -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_action_modal_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#define HOST
#endif
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) { (void)face; return (int32_t)value.length * font / 2; }
HOST int32_t MeasureGlyphLineHeight(int32_t font,
    String face) { (void)face; return font; }
HOST int32_t ImageWidth(String path) { (void)path; return 0; }
HOST int32_t ImageHeight(String path) { (void)path; return 0; }
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius;
    (void)tint;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds,
    float radius, int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width;
    (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    (void)clip;
}
int main(void) {
    for (int phase = 0; phase < 6; phase++)
        assert(Frame() == phase);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/action_modal_widget_test.go" <<'GO'
package ziran
import "testing"
type modalHost struct{}
func (modalHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (modalHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func TestActionModal(t *testing.T) {
    SetFontMetricsHost(modalHost{})
    for phase := int32(0); phase < 6; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
