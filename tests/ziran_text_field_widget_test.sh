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
#import "text_field_widget"
#import "text_input_props"
#import "tree"
#import "tree_input"

phase :: i32 #global
value :: string #global
cursor :: i32 #global
anchor :: i32 #global
scroll_x :: i32 #global
focused :: bool #global

Frame :: () -> i32 #export {
    if phase == 0 {
        value = "AéB"
        cursor = 4
        anchor = 4
    }
    props: TextFieldProps
    props.key = (u64)77
    props.bounds = (Rectangle){20.0, 20.0, 220.0, 36.0}
    props.value = value
    props.label = "Name"
    props.placeholder = "Enter name"
    props.cursor = cursor
    props.anchor = anchor
    props.scroll_x = scroll_x
    props.focused = focused
    props.focus_id = 77
    props.max_bytes = 5
    props.max_codepoints = 4
    if phase == 1 { props.input.text = "Z" }
    if phase == 2 { props.input.left = true }
    if phase == 3 { props.input.backspace = true }
    if phase == 4 { props.input.select_all = true }
    if phase == 5 { props.input.text = "Q" }
    if phase == 6 {
        props.secure = true
        props.display_value = "•"
    }
    if phase == 7 {
        props.disabled = true
        props.input.text = "X"
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 300.0, 100.0})
    result: TextFieldResult = TextField(props)
    if !TreeFinish() || result.node != 1 ||
        TreeNodeAt(result.node).semantic_label != "Name" ||
        TreeNodeAt(result.node).semantic_kind !=
            (SemanticKind)SemanticTextField { return -20 }
    if phase == 0 {
        if result.focused || result.edit.changed { return -1 }
        TreePointerUpdate((PointerFrame){220.0, 38.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){220.0, 38.0,
            false, false, true})
    } else if phase == 1 {
        if !result.focused || !result.edit.changed ||
            result.edit.start != 4 || result.edit.end != 4 ||
            result.edit.replacement != "Z" || result.cursor != 5 {
            return -2
        }
        value = "AéBZ"
    } else if phase == 2 {
        if result.cursor != 4 || result.anchor != 4 ||
            result.edit.changed { return -3 }
    } else if phase == 3 {
        if !result.edit.changed || result.edit.start != 3 ||
            result.edit.end != 4 || result.cursor != 3 {
            return -4
        }
        value = "AéZ"
    } else if phase == 4 {
        if result.cursor != 4 || result.anchor != 0 ||
            result.edit.changed { return -5 }
    } else if phase == 5 {
        if !result.edit.changed || result.edit.start != 0 ||
            result.edit.end != 4 || result.edit.replacement != "Q" ||
            result.cursor != 1 { return -6 }
        value = "Q"
    } else if phase == 6 {
        if !result.focused || result.cursor != 1 ||
            result.edit.changed { return -7 }
    } else if phase == 7 {
        if result.focused || result.edit.changed { return -8 }
    }
    cursor = result.cursor
    anchor = result.anchor
    scroll_x = result.scroll_x
    focused = result.focused
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
    "$repo/tests/ziran_text_field_widget_test.c" \
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
    for (int phase = 0; phase < 8; phase++)
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
        cat > "$output/text_field_widget_test.go" <<'GO'
package ziran
import "testing"
type fieldHost struct{}
func (fieldHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (fieldHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (fieldHost) TextSlice(source string, start,
    length int32) string { return source[start:start+length] }
func TestTextField(t *testing.T) {
    SetFontMetricsHost(fieldHost{})
    SetTextWidgetHost(fieldHost{})
    for phase := int32(0); phase < 8; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
