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

#import "geometry"
#import "semantic"
#import "text_field_widget"
#import "text_input"
#import "text_input_props"
#import "tree"
#import "tree_draw"
#import "tree_input"

phase: s32;
value: string;
cursor: s32;
anchor: s32;
scroll_x: s32;
focused: bool;
ime_state: CompositionState;

#program_export
Frame :: () -> s32 {
    if phase == 0 {
        value = "AéB"
        cursor = 4
        anchor = 4
    }
    if phase == 8 {
        value = "AéB"
        cursor = 4
        anchor = 1
        focused = true
    }
    if phase == 13 { anchor = 1 }
    if phase == 17 { anchor = 0 }
    props: TextFieldProps
    props.key = cast(u64)77
    props.bounds = Rectangle.{20.0, 20.0, 220.0, 36.0}
    props.value = value
    props.label = "Name"
    props.placeholder = "Enter name"
    props.cursor = cursor
    props.anchor = anchor
    props.scroll_x = scroll_x
    props.focused = focused
    props.composition = ime_state
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
    if phase == 8 { props.input.copy = true }
    if phase == 9 { props.input.cut = true }
    if phase == 10 {
        props.input.paste = true
        props.input.paste_text = "éB"
    }
    if phase == 11 { props.input.copy = true }
    if phase == 12 {
        props.secure = true
        props.display_value = "•••"
        props.input.copy = true
    }
    if phase == 13 {
        props.read_only = true
        props.input.copy = true
    }
    if phase == 14 {
        props.read_only = true
        props.input.cut = true
    }
    if phase == 15 {
        props.read_only = true
        props.input.paste = true
        props.input.paste_text = "Q"
    }
    if phase == 16 {
        props.input.paste = true
        props.input.paste_text = "Q"
    }
    if phase == 17 {
        props.input.paste = true
        props.input.paste_text = "\n"
    }
    if phase == 18 {
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionStart
        props.input.composition_event.text = "é"
        props.input.composition_event.cursor = 1
        props.input.composition_event.selection_length = 99
        props.input.text = "X"
    }
    if phase == 19 {
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionUpdate
        props.input.composition_event.text = "éx"
        props.input.composition_event.cursor = 3
    }
    if phase == 20 {
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionCommit
        props.input.composition_event.text = "Q"
        props.input.text = "X"
    }
    if phase == 21 {
        props.read_only = true
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionStart
        props.input.composition_event.text = "A"
    }
    if phase == 22 || phase == 24 {
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionStart
        props.input.composition_event.text = "B"
    }
    if phase == 23 {
        props.input.composition_event.phase =
            cast(CompositionPhase)CompositionCancel
    }
    if phase == 25 || phase == 26 { props.input.escape = true }
    BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 300.0, 100.0})
    result: TextFieldResult = TextField(TestSession(), props)
    if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || result.node != 1 ||
        TreeNodeAt(TestSession(), result.node).semantic_label != "Name" ||
        TreeNodeAt(TestSession(), result.node).semantic_kind !=
            cast(SemanticKind)SemanticTextField { return -20 }
    if phase == 0 {
        if result.focused || result.edit.changed { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{220.0, 38.0,
            true, true, false})
        TreePointerUpdate(TestSession(), PointerFrame.{220.0, 38.0,
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
    } else if phase == 8 {
        if !result.clipboard_write ||
            result.clipboard_text != "éB" ||
            result.edit.changed { return -9 }
    } else if phase == 9 {
        if !result.clipboard_write ||
            result.clipboard_text != "éB" ||
            !result.edit.changed || result.edit.start != 1 ||
            result.edit.end != 4 || result.cursor != 1 {
            return -10
        }
        value = "A"
    } else if phase == 10 {
        if result.clipboard_write || !result.edit.changed ||
            result.edit.start != 1 || result.edit.end != 1 ||
            result.edit.replacement != "éB" ||
            result.cursor != 4 { return -11 }
        value = "AéB"
    } else if phase == 11 {
        if !result.clipboard_write ||
            result.clipboard_text != "AéB" ||
            result.edit.changed { return -12 }
    } else if phase == 12 {
        if result.clipboard_write || result.edit.changed {
            return -13
        }
    } else if phase == 13 {
        if !result.clipboard_write ||
            result.clipboard_text != "éB" ||
            result.edit.changed { return -14 }
    } else if phase == 14 {
        if result.clipboard_write || result.edit.changed {
            return -15
        }
    } else if phase == 15 {
        if result.clipboard_write || result.edit.changed {
            return -16
        }
    } else if phase == 16 {
        if result.clipboard_write || !result.edit.changed ||
            result.edit.start != 1 || result.edit.end != 4 ||
            result.edit.replacement != "Q" ||
            result.cursor != 2 { return -17 }
        value = "AQ"
    } else if phase == 17 {
        if result.clipboard_write || result.edit.changed ||
            result.cursor != 2 { return -18 }
    } else if phase == 18 {
        sample: CompositionEvent
        sample.phase = cast(CompositionPhase)CompositionUpdate
        sample.text = "éx"
        sample.cursor = 1
        sample.selection_length = 2
        boundary: CompositionTransition = CompositionTransitionFor(
            CompositionState.{}, sample, true, false)
        if boundary.state.cursor != 0 ||
            boundary.state.selection_length != 3 {
            return -28
        }
        if !result.composition.active ||
            result.composition.text != "é" ||
            result.composition.cursor != 0 ||
            result.composition.selection_length != 2 ||
            !result.composition_changed ||
            result.edit.changed { return -19 }
    } else if phase == 19 {
        if !result.composition.active ||
            result.composition.text != "éx" ||
            result.composition.cursor != 3 ||
            result.edit.changed { return -20 }
    } else if phase == 20 {
        if result.composition.active ||
            !result.composition_changed ||
            !result.edit.changed || result.edit.start != 0 ||
            result.edit.end != 2 ||
            result.edit.replacement != "Q" ||
            result.cursor != 1 { return -21 }
        value = "Q"
    } else if phase == 21 {
        if result.composition.active || result.edit.changed {
            return -22
        }
    } else if phase == 22 {
        if !result.composition.active ||
            result.composition.text != "B" {
            return -23
        }
    } else if phase == 23 {
        if result.composition.active ||
            !result.composition_changed ||
            result.edit.changed { return -24 }
    } else if phase == 24 {
        if !result.composition.active { return -25 }
    } else if phase == 25 {
        if result.composition.active ||
            !result.composition_changed ||
            result.escaped || !result.focused { return -26 }
    } else if phase == 26 {
        if !result.escaped || result.focused {
            return -27
        }
    }
    cursor = result.cursor
    anchor = result.anchor
    scroll_x = result.scroll_x
    focused = result.focused
    ime_state = result.composition
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
    for (int phase = 0; phase < 27; phase++)
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
        cat > "$output/text_field_widget_test.go" <<'GO'
package ziran
import "testing"
type fieldHost struct{ preedit, underline int }
func (*fieldHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (*fieldHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func (*fieldHost) ImageWidth(path string) int32 { return 0 }
func (*fieldHost) ImageHeight(path string) int32 { return 0 }
func (h *fieldHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.Height == 2 && color.R == 59 &&
       color.G == 130 && color.B == 246 { h.underline++ }
}
func (*fieldHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {}
func (*fieldHost) RasterLine(line Rectangle, color Color) {}
func (*fieldHost) RasterText(value string, x, y, font int32,
    color Color) {}
func (h *fieldHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value == "é" { h.preedit++ }
}
func (*fieldHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {}
func TestTextField(t *testing.T) {
    h := &fieldHost{}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := int32(0); phase < 27; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
    if h.preedit == 0 || h.underline == 0 {
        t.Fatalf("preedit paint %d underline %d",
            h.preedit, h.underline)
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
