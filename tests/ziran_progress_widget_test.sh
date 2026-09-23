#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "control_props"
#import "geometry"
#import "progress"
#import "progress_props"
#import "progress_style"
#import "progress_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"

Answer :: () -> i32 #export {
    props: ProgressProps
    props.bounds = (Rectangle){10.0, 20.0, 100.0, 20.0}
    props.min = 0
    props.max = 100
    props.value = 25
    props.label = "50%"
    props.class_name = 7
    defaults: ProgressFaces
    defaults.scale = 1.0
    defaults.fallback_font = 16
    rules: StyleRules
    rules.count = 320
    track: StyleRule
    track.selector = StyleDefaultSelector()
    track.selector.kind = StyleKindProgress()
    track.selector.class_name = 7
    track.selector.role = ProgressTrackRole()
    track.selector.tone = (i32)ButtonToneNeutral
    track.style.fields = (u32)StyleBackground | (u32)StyleBorder |
        (u32)StyleBorderWidth | (u32)StyleRadius
    track.style.background = (u32)0x11223344
    track.style.border = (u32)0x99aabbcc
    track.style.border_width = 2.0
    track.style.radius = 5.0
    rules.items[0] = track
    filled: StyleRule
    filled.selector = StyleDefaultSelector()
    filled.selector.kind = StyleKindProgress()
    filled.selector.class_name = 7
    filled.selector.role = ProgressFillRole()
    filled.selector.tone = (i32)ButtonToneAccent
    filled.style.fields = (u32)StyleBackground
    filled.style.background = (u32)0x55667788
    rules.items[1] = filled
    label: StyleRule
    label.selector = StyleDefaultSelector()
    label.selector.kind = StyleKindProgress()
    label.selector.class_name = 7
    label.selector.role = ProgressLabelRole()
    label.selector.tone = (i32)ButtonToneNeutral
    label.style.fields = (u32)StyleForeground | (u32)StyleFontSize |
        (u32)StyleOpacity | (u32)StyleTypeface
    label.style.foreground = (u32)0x10203080
    label.style.font_size = 14.0
    label.style.opacity = 0.5
    label.style.typeface = "body"
    rules.items[2] = label
    ignored: StyleRule = track
    ignored.selector.class_name = 99
    ignored.layer = 100
    ignored.style.background = (u32)0xdeadbeef
    rules.items[3] = ignored
    faces: ProgressFaces = ProgressFacesFor(rules, props.class_name, defaults)
    if faces.track.value.background != (u32)0x11223344 ||
        faces.fill.value.background != (u32)0x55667788 ||
        faces.label.value.font_size != 14.0 { return 0 }
    InstallStyleRules(rules)
    props.key = (u64)17
    BeginTree((u64)10, (Rectangle){0.0, 0.0, 200.0, 100.0})
    Progress(props)
    if !EndTree() || TreeCount() != 2 { return 0 }
    props.value = 0
    props.label = ""
    rules.count = 0
    defaults.track.value.background = (u32)0x11223344
    defaults.track.value.radius = 5.0
    prepared: PreparedProgress = PrepareProgress(props, defaults)
    if prepared.font != 16 || prepared.paint.layout.fill_bounds.width != 0.0 {
        return 0
    }
    PaintProgressFromRules(props, rules, defaults)
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" -I"$repo/../ziran/include" \
    "$repo/tests/ziran_progress_widget_test.c" \
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
static int measures, draws;
HOST int32_t MeasureGlyphWidth(String value, int32_t font, String typeface) {
    assert(value.length == 3 && font == 14 && typeface.length == 4);
    measures++;
    return 20;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    assert(font == 14 && typeface.length == 4);
    measures++;
    return 10;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)color;
    assert(bounds.x == 10 && bounds.y == 20 && radius == 0.25f &&
           segments == 12);
    draws++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
    assert(width == 2.0f);
    draws++;
}
HOST void RasterText(String value, int32_t x, int32_t y, int32_t font,
    Color color) {
    (void)color;
    assert(value.length == 3 && x == 41 && y == 25 && font == 14);
    draws++;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    assert(0 && "Progress should not draw clipped text");
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    assert(0 && "Progress should not draw separator lines");
}
HOST void RasterImage(String path, uint32_t texture_id,
    Rectangle source, Rectangle destination, Rectangle clip,
    Vector2 origin, float rotation, float radius, Color tint) {
    (void)path; (void)texture_id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    assert(0 && "Progress should not draw images");
}
int main(void) {
    assert(Answer() == 42 && measures == 2 && draws == 5);
    return 0;
}
C

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict --root "$work" \
            --module-path "$module_dir" -o "$output" "$module"
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
            cat > "$output/progress_widget_test.go" <<'GO'
package ziran
import "testing"
type widgetHost struct { t *testing.T; measures, draws int }
func (host *widgetHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "50%" || font != 14 || typeface != "body" { host.t.Fatal("width") }
    host.measures++
    return 20
}
func (host *widgetHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "body" { host.t.Fatal("line height") }
    host.measures++
    return 10
}
func (host *widgetHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || radius != 0.25 ||
       segments != 12 { host.t.Fatal("shape") }
    host.draws++
}
func (host *widgetHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if width != 2 { host.t.Fatal("outline") }
    host.draws++
}
func (host *widgetHost) RasterText(value string, x int32, y int32,
    font int32, color Color) {
    if value != "50%" || x != 41 || y != 25 || font != 14 {
        host.t.Fatal("label")
    }
    host.draws++
}
func (host *widgetHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    host.t.Fatal("Progress should not draw clipped text")
}
func (host *widgetHost) RasterLine(line Rectangle, color Color) {
    host.t.Fatal("Progress should not draw separator lines")
}
func (host *widgetHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    host.t.Fatal("Progress should not draw images")
}
func TestProgressWidget(t *testing.T) {
    host := &widgetHost{t: t}
    SetFontMetricsHost(host)
    SetRasterShapeHost(host)
    SetRasterTextHost(host)
    SetRasterHost(host)
    SetPaintQueueHost(host)
    if App_Answer() != 42 || host.measures != 2 || host.draws != 5 {
        t.Fatal("progress composition")
    }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
