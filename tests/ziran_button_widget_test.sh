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

#import "button_props"
#import "button_widget"
#import "control_props"
#import "geometry"
#import "image_props"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "tree_input"

using ImageFit;
using ButtonTone;
using IconPlacement;
using StyleField;
using FrameStatus;

phase: s32;

#program_export
Frame :: () -> s32 {
    props: ButtonProps
    props.key = cast(u64)7
    props.id = 7
    props.class_name = 9
    props.label = "Run"
    props.bounds = Rectangle.{10.0, 20.0, 80.0, 30.0}
    props.tone = cast(ButtonTone)ButtonToneAccent
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        rule: StyleRule
        rule.selector = StyleDefaultSelector()
        rule.selector.kind = StyleKindButton()
        rule.selector.class_name = 9
        rule.style.fields = cast(u32)StyleBackground |
            cast(u32)StyleForeground | cast(u32)StyleRadius |
            cast(u32)StyleOpacity | cast(u32)StyleFontSize
        rule.style.background = cast(u32)0x123456ff
        rule.style.foreground = cast(u32)0xaabbccff
        rule.style.radius = 4.0
        rule.style.opacity = 0.5
        rule.style.font_size = 14.0
        rules.items[0] = rule
        InstallStyleRules(rules)
        BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
        if Button(TestSession(), props) != 0 || EndFrame(TestSession()) != cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).semantic_label != "Run" { return -1 }
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 30.0, true, true, false})
        phase = 1
        return 0
    }
    if phase == 1 {
        TreePointerUpdate(TestSession(), PointerFrame.{20.0, 30.0, false, false, true})
        BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
        clicked: s32 = Button(TestSession(), props)
        if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || clicked != 1 { return -2 }
        phase = 2
        return 42
    }
    if phase == 3 {
        props.image.asset_path = "badge.png"
        props.image.fit = cast(ImageFit)ImageFitContain
        props.icon_placement = cast(IconPlacement)IconPlacementTrailing
        BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
        if Button(TestSession(), props) != 0 || EndFrame(TestSession()) != cast(FrameStatus)FrameOk ||
            TreeNodeAt(TestSession(), 1).semantic_label != "Run" { return -4 }
        phase = 4
        return 44
    }
    props.disabled = true
    BeginFrame(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    clicked: s32 = Button(TestSession(), props)
    if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || clicked != 0 { return -3 }
    TreePointerUpdate(TestSession(), PointerFrame.{20.0, 30.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{20.0, 30.0, false, false, true})
    phase = 3
    return 43
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
    "$repo/tests/ziran_button_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#include <cstring>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#include <string.h>
#define HOST
#endif
static int fills, labels, images;
HOST int32_t ImageWidth(String path) {
    assert(path.length == 9 && memcmp(path.data, "badge.png", 9) == 0);
    return 32;
}
HOST int32_t ImageHeight(String path) {
    assert(path.length == 9 && memcmp(path.data, "badge.png", 9) == 0);
    return 16;
}
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    assert(value.length == 3 && memcmp(value.data, "Run", 3) == 0);
    assert(font == 14 && typeface.length == 0);
    return 21;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 0);
    return 12;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 10 && bounds.y == 20 &&
           bounds.width == 80 && bounds.height == 30);
    assert(radius == 4 && segments == 12);
    assert(color.r == 0x12 && color.g == 0x34 &&
           color.b == 0x56 && color.a == 127);
    fills++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
    assert(0 && "unexpected outline");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    assert(0 && "Button label must be clipped");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    assert(value.length == 3 && memcmp(value.data, "Run", 3) == 0);
    assert(x == (images == 0 ? 39 : 26) && y == 29 && font == 14);
    assert(clip.x == 10 && clip.y == 20 &&
           clip.width == 80 && clip.height == 30);
    assert(color.r == 0xaa && color.g == 0xbb &&
           color.b == 0xcc && color.a == 127);
    labels++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    assert(0 && "unexpected line");
}
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    assert(path.length == 9 && memcmp(path.data, "badge.png", 9) == 0);
    assert(id == 0 && source.x == 0 && source.y == 0 &&
           source.width == 32 && source.height == 16);
    assert(destination.x == 55.5f && destination.y == 30.5f &&
           destination.width == 18 && destination.height == 9);
    assert(clip.x == 10 && clip.y == 20 &&
           clip.width == 80 && clip.height == 30);
    assert(origin.x == 0 && origin.y == 0 && rotation == 0 && radius == 4);
    assert(tint.r == 255 && tint.g == 255 &&
           tint.b == 255 && tint.a == 127);
    images++;
}
int main(void) {
    assert(Frame() == 0 && fills == 1 && labels == 1);
    assert(Frame() == 42 && fills == 2 && labels == 2);
    assert(Frame() == 43 && fills == 3 && labels == 3);
    assert(Frame() == 44 && fills == 4 && labels == 4 && images == 1);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/button_widget_test.go" <<'GO'
package ziran
import "testing"
type buttonTestHost struct { t *testing.T; fills, labels, images int }
func (h *buttonTestHost) ImageWidth(path string) int32 {
    if path != "badge.png" { h.t.Fatal("button image width") }
    return 32
}
func (h *buttonTestHost) ImageHeight(path string) int32 {
    if path != "badge.png" { h.t.Fatal("button image height") }
    return 16
}
func (h *buttonTestHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Run" || font != 14 || typeface != "" {
        h.t.Fatal("button width")
    }
    return 21
}
func (h *buttonTestHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("button height") }
    return 12
}
func (h *buttonTestHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 80 ||
       bounds.Height != 30 || radius != 4 || segments != 12 ||
       color.R != 0x12 || color.G != 0x34 ||
       color.B != 0x56 || color.A != 127 { h.t.Fatal("button fill") }
    h.fills++
}
func (h *buttonTestHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *buttonTestHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped button label") }
func (h *buttonTestHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    expectedX := int32(39)
    if h.images != 0 { expectedX = 26 }
    if value != "Run" || x != expectedX || y != 29 || font != 14 ||
       clip.X != 10 || clip.Y != 20 ||
       clip.Width != 80 || clip.Height != 30 ||
       color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 127 { h.t.Fatal("button label") }
    h.labels++
}
func (h *buttonTestHost) RasterLine(line Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *buttonTestHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    if path != "badge.png" || id != 0 ||
       source.X != 0 || source.Y != 0 ||
       source.Width != 32 || source.Height != 16 ||
       destination.X != 55.5 || destination.Y != 30.5 ||
       destination.Width != 18 || destination.Height != 9 ||
       clip.X != 10 || clip.Y != 20 ||
       clip.Width != 80 || clip.Height != 30 ||
       origin.X != 0 || origin.Y != 0 || rotation != 0 || radius != 4 ||
       tint.R != 255 || tint.G != 255 ||
       tint.B != 255 || tint.A != 127 { h.t.Fatal("button image") }
    h.images++
}
func TestButtonWidget(t *testing.T) {
    h := &buttonTestHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    SetImageRasterHost(h)
    if App_Frame() != 0 || h.fills != 1 || h.labels != 1 {
        t.Fatal("first frame")
    }
    if App_Frame() != 42 || h.fills != 2 || h.labels != 2 {
        t.Fatal("activation")
    }
    if App_Frame() != 43 || h.fills != 3 || h.labels != 3 {
        t.Fatal("disabled")
    }
    if App_Frame() != 44 || h.fills != 4 || h.labels != 4 || h.images != 1 {
        t.Fatal("image")
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
