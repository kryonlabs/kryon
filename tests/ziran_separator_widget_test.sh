#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "control_props"
#import "geometry"
#import "separator"
#import "separator_props"
#import "separator_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"

phase: s32;

#program_export
Frame :: () -> s32 {
    if phase == 1 {
        if !EndTree() || TreeCount() != 4 { return -1 }
        phase = 2
        return 1
    }
    vertical: SeparatorProps
    vertical.bounds = Rectangle.{10.0, 20.0, 20.0, 40.0}
    vertical.vertical = true
    vertical.class_name = 7
    vertical.key = cast(u64)11
    if phase == 2 {
        Separator(vertical)
        phase = 3
        return 2
    }
    rules: StyleRules
    rules.count = 2
    line: StyleRule
    line.selector = StyleDefaultSelector()
    line.selector.kind = StyleKindSeparator()
    line.selector.class_name = 7
    line.selector.role = SeparatorLineRole()
    line.style.fields = cast(u32)StyleBackground
    line.style.background = cast(u32)0x11223344
    rules.items[0] = line
    label_rule: StyleRule
    label_rule.selector = StyleDefaultSelector()
    label_rule.selector.kind = StyleKindSeparator()
    label_rule.selector.class_name = 7
    label_rule.selector.role = SeparatorLabelRole()
    label_rule.style.fields = cast(u32)StyleForeground |
        cast(u32)StyleFontSize | cast(u32)StyleGap | cast(u32)StyleOpacity
    label_rule.style.foreground = cast(u32)0xaabbccdd
    label_rule.style.font_size = 14.0
    label_rule.style.gap = 8.0
    label_rule.style.opacity = 0.5
    rules.items[1] = label_rule
    InstallStyleRules(rules)
    labelled: SeparatorProps
    labelled.bounds = Rectangle.{40.0, 20.0, 100.0, 20.0}
    labelled.label = "A"
    labelled.class_name = 7
    labelled.key = cast(u64)12
    BeginTree(cast(u64)10, Rectangle.{0.0, 0.0, 200.0, 100.0})
    Separator(vertical)
    Separator(labelled)
    Bullet(Rectangle.{150.0, 20.0, 20.0, 20.0})
    phase = 1
    return 0
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
    "$repo/tests/ziran_separator_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native.zi" <<'ZI'
#import "control_props"
#import "geometry"
#import "separator"
#import "separator_props"
#import "separator_widget"
#import "style"
#import "style_sheet"

#program_export
Answer :: () -> s32 {
    rules: StyleRules
    rules.count = 3
    line: StyleRule
    line.selector = StyleDefaultSelector()
    line.selector.kind = StyleKindSeparator()
    line.selector.role = SeparatorLineRole()
    line.style.fields = cast(u32)StyleBackground
    line.style.background = cast(u32)0x11223344
    rules.items[0] = line
    label: StyleRule
    label.selector = StyleDefaultSelector()
    label.selector.kind = StyleKindSeparator()
    label.selector.role = SeparatorLabelRole()
    label.style.fields = cast(u32)StyleForeground | cast(u32)StyleGap
    label.style.foreground = cast(u32)0xaabbccdd
    label.style.gap = 8.0
    rules.items[1] = label
    bullet: StyleRule
    bullet.selector = StyleDefaultSelector()
    bullet.selector.kind = StyleKindSeparator()
    bullet.selector.role = SeparatorBulletRole()
    bullet.style.fields = cast(u32)StyleForeground | cast(u32)StyleIconSize
    bullet.style.foreground = cast(u32)0x123456ff
    bullet.style.icon_size = 8.0
    rules.items[2] = bullet
    InstallStyleRules(rules)
    vertical: SeparatorProps
    vertical.bounds = Rectangle.{10.0, 20.0, 20.0, 40.0}
    vertical.vertical = true
    Separator(vertical)
    labelled: SeparatorProps
    labelled.bounds = Rectangle.{40.0, 20.0, 100.0, 20.0}
    labelled.label = "A"
    Separator(labelled)
    Bullet(Rectangle.{150.0, 20.0, 20.0, 20.0})
    return 42
}
ZI

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "native.hpp"
#include <cassert>
#include <cstring>
#define HOST extern "C"
#else
#include "native.h"
#include <assert.h>
#include <string.h>
#define HOST
#endif
static int lines, texts, measures, bullets;
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)typeface;
    assert(value.length == 1 && value.data[0] == 'A' && font == 14);
    measures++;
    return 10;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface;
    assert(0 && "Separator should not measure line height");
    return 0;
}
HOST void RasterLine(Rectangle line, Color color) {
    assert(color.r == 0x11 && color.g == 0x22 &&
           color.b == 0x33 && color.a == 0x44);
    if(lines == 0)
        assert(line.x == 20 && line.y == 20 &&
               line.width == 0 && line.height == 40);
    else
        assert(line.x == 58 && line.y == 30 &&
               line.width == 82 && line.height == 0);
    lines++;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    assert(value.length == 1 && value.data[0] == 'A');
    assert(x == 40 && y == 23 && font == 14);
    assert(color.r == 0xaa && color.g == 0xbb &&
           color.b == 0xcc && color.a == 0xdd);
    texts++;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    assert(0 && "Separator should not draw clipped text");
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    assert(bounds.x == 156 && bounds.y == 26 &&
           bounds.width == 8 && bounds.height == 8);
    assert(radius == 0.5f && segments == 32);
    assert(color.r == 0x12 && color.g == 0x34 &&
           color.b == 0x56 && color.a == 0xff);
    bullets++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments;
    (void)width; (void)color;
    assert(0 && "Separator should not draw rounded outlines");
}
HOST void RasterImage(String path, uint32_t texture_id,
    Rectangle source, Rectangle destination, Rectangle clip,
    Vector2 origin, float rotation, float radius, Color tint) {
    (void)path; (void)texture_id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0 && "Separator should not draw images");
}
int main(void) {
    assert(Answer() == 42 && lines == 2 && texts == 1 &&
           measures == 1 && bullets == 1);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/native.zi"
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
        cat > "$output/separator_widget_test.go" <<'GO'
package ziran
import "testing"
type separatorHost struct { t *testing.T; lines, texts, measures, bullets int }
func (h *separatorHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "A" || font != 14 { h.t.Fatal("width") }
    h.measures++
    return 10
}
func (h *separatorHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { h.t.Fatal("line height"); return 0 }
func (h *separatorHost) RasterLine(line Rectangle, color Color) {
    if color.R != 0x11 || color.G != 0x22 ||
       color.B != 0x33 || color.A != 0x44 { h.t.Fatal("color") }
    if h.lines == 0 {
        if line.X != 20 || line.Y != 20 ||
           line.Width != 0 || line.Height != 40 { h.t.Fatal("vertical") }
    } else if line.X != 58 || line.Y != 30 ||
       line.Width != 82 || line.Height != 0 { h.t.Fatal("label line") }
    h.lines++
}
func (h *separatorHost) RasterText(value string, x, y, font int32,
    color Color) {
    if value != "A" || x != 40 || y != 23 || font != 14 ||
       color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 0xdd { h.t.Fatal("label") }
    h.texts++
}
func (h *separatorHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    h.t.Fatal("Separator should not draw clipped text")
}
func (h *separatorHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 156 || bounds.Y != 26 ||
       bounds.Width != 8 || bounds.Height != 8 ||
       radius != 0.5 || segments != 32 ||
       color.R != 0x12 || color.G != 0x34 ||
       color.B != 0x56 || color.A != 0xff { h.t.Fatal("bullet") }
    h.bullets++
}
func (h *separatorHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("Separator should not draw rounded outlines")
}
func (h *separatorHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    h.t.Fatal("Separator should not draw images")
}
func TestSeparatorWidget(t *testing.T) {
    host := &separatorHost{t: t}
    SetFontMetricsHost(host)
    SetRasterHost(host)
    SetRasterTextHost(host)
    SetRasterShapeHost(host)
    SetPaintQueueHost(host)
    if Native_Answer() != 42 || host.lines != 2 ||
       host.texts != 1 || host.measures != 1 || host.bullets != 1 {
        t.Fatal("separator composition")
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
