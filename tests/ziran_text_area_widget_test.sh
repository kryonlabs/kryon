#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "semantic"
#import "text_area_widget"
#import "text_input_props"
#import "tree"
#import "tree_input"

phase :: i32 #global
value :: string #global
cursor :: i32 #global
anchor :: i32 #global
scroll_y :: i32 #global
focused :: bool #global
selecting :: bool #global
preferred_x :: i32 #global
preferred_x_valid :: bool #global

Frame :: () -> i32 #export {
    if phase == 0 {
        value = "Aé\nBC"
        cursor = 6
        anchor = 6
    }
    if phase == 12 {
        value = "a\nb\nc\nd\ne\nf\ng"
        cursor = 0
        anchor = 0
        scroll_y = 0
    }
    if phase == 14 {
        value = "abcd"
        cursor = 0
        anchor = 0
        scroll_y = 0
        selecting = false
    }
    props: TextAreaProps
    props.key = (u64)77
    props.bounds = (Rectangle){20.0, 20.0, 100.0, 52.0}
    props.value = value
    props.label = "Notes"
    props.placeholder = "Write notes"
    props.cursor = cursor
    props.anchor = anchor
    props.scroll_y = scroll_y
    props.focused = focused
    props.selecting = selecting
    props.preferred_x = preferred_x
    props.preferred_x_valid = preferred_x_valid
    props.focus_id = 77
    props.max_bytes = 8
    props.max_codepoints = 8
    if phase == 1 { props.input.text = "Z" }
    if phase == 2 { props.input.left = true }
    if phase == 3 { props.input.backspace = true }
    if phase == 4 { props.input.up = true }
    if phase == 5 { props.input.down = true }
    if phase == 6 { props.input.enter = true }
    if phase == 7 { props.input.select_all = true }
    if phase == 8 { props.input.text = "Q\nR" }
    if phase == 9 {
        props.read_only = true
        props.input.text = "X"
    }
    if phase == 10 {
        props.disabled = true
        props.input.text = "X"
    }
    if phase == 12 { props.input.wheel = -1.0 }
    if phase == 13 {
        props.input.focus = true
        props.input.page_down = true
    }
    if phase == 18 { props.input.text = "Y" }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 300.0, 140.0})
    result: TextAreaResult = TextArea(props)
    if !TreeFinish() || result.node != 1 ||
        TreeNodeAt(result.node).semantic_label != "Notes" ||
        TreeNodeAt(result.node).semantic_kind !=
            (SemanticKind)SemanticTextArea { return -20 }
    if phase == 0 {
        if result.focused || result.edit.changed { return -1 }
        if TextAreaRowCount(value, 0, 14, "") != 2 ||
            TextAreaNextRow(value, 0, 0, 14, "").end != 3 ||
            TextAreaCaretFor(value, 5, 0, 14, "", 18).row_index != 1 {
            return -19
        }
        TreePointerUpdate((PointerFrame){60.0, 49.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){60.0, 49.0,
            false, false, true})
    } else if phase == 1 {
        if !result.focused || !result.edit.changed ||
            result.edit.start != 6 || result.edit.end != 6 ||
            result.edit.replacement != "Z" || result.cursor != 7 {
            return -2
        }
        value = "Aé\nBCZ"
    } else if phase == 2 {
        if result.cursor != 6 || result.anchor != 6 ||
            result.edit.changed { return -3 }
    } else if phase == 3 {
        if !result.edit.changed || result.edit.start != 5 ||
            result.edit.end != 6 || result.cursor != 5 {
            return -4
        }
        value = "Aé\nBZ"
    } else if phase == 4 {
        if result.cursor != 1 || result.anchor != 1 ||
            result.edit.changed { return -5 }
    } else if phase == 5 {
        if result.cursor != 5 || result.anchor != 5 ||
            result.edit.changed { return -6 }
    } else if phase == 6 {
        if !result.edit.changed || result.edit.start != 5 ||
            result.edit.end != 5 ||
            result.edit.replacement != "\n" ||
            result.cursor != 6 { return -7 }
        value = "Aé\nB\nZ"
    } else if phase == 7 {
        if result.cursor != 7 || result.anchor != 0 ||
            result.edit.changed { return -8 }
    } else if phase == 8 {
        if !result.edit.changed || result.edit.start != 0 ||
            result.edit.end != 7 ||
            result.edit.replacement != "Q\nR" ||
            result.cursor != 3 { return -9 }
        value = "Q\nR"
    } else if phase == 9 {
        if !result.focused || result.edit.changed { return -10 }
    } else if phase == 10 {
        if result.focused || result.edit.changed { return -11 }
    } else if phase == 11 {
        row: TextAreaRow = TextAreaNextRow(
            "one two three", 0, 28, 14, "")
        if row.end != 4 || row.next != 4 ||
            !row.soft_wrap ||
            TextAreaRowCount("one two three",
                28, 14, "") != 4 { return -12 }
    } else if phase == 12 {
        if result.scroll_y != 54 || result.cursor != 0 {
            return -13
        }
    } else if phase == 13 {
        if !result.focused || result.cursor != 4 ||
            result.scroll_y != 36 { return -14 }
    } else if phase == 14 {
        TreePointerUpdate((PointerFrame){31.0, 36.0,
            true, true, false})
    } else if phase == 15 {
        if result.cursor != 0 || result.selecting {
            return -15
        }
        TreePointerUpdate((PointerFrame){60.0, 36.0,
            true, false, false})
    } else if phase == 16 {
        if !result.selecting || result.anchor != 0 ||
            result.cursor != 4 { return -16 }
        TreePointerUpdate((PointerFrame){60.0, 36.0,
            false, false, true})
    } else if phase == 17 {
        if result.selecting || result.anchor != 0 ||
            result.cursor != 4 { return -17 }
    } else if phase == 18 {
        if !result.edit.changed || result.edit.start != 0 ||
            result.edit.end != 4 ||
            result.edit.replacement != "Y" ||
            result.cursor != 1 { return -18 }
    }
    cursor = result.cursor
    anchor = result.anchor
    scroll_y = result.scroll_y
    focused = result.focused
    selecting = result.selecting
    preferred_x = result.preferred_x
    preferred_x_valid = result.preferred_x_valid
    old: i32 = phase
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_text_area_widget_test.c" \
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
HOST String TextSlice(String source, int32_t start, int32_t length) {
    assert(start >= 0 && length >= 0 &&
           (size_t)start + (size_t)length <= source.length);
    String result = {source.data + start, (size_t)length};
    return result;
}
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
    for (int phase = 0; phase < 19; phase++)
        assert(Frame() == phase);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
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
        cat > "$output/text_area_widget_test.go" <<'GO'
package ziran
import "testing"
type fieldHost struct{}
func (fieldHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (fieldHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (fieldHost) TextSlice(source string, start,
    length int32) string { return source[start:start+length] }
func TestTextArea(t *testing.T) {
    SetFontMetricsHost(fieldHost{})
    SetTextWidgetHost(fieldHost{})
    for phase := int32(0); phase < 19; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
