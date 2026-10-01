#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/progress_widget_behavior.zi
portable=$repo/tests/progress_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphWidth=progress_widget_metrics_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=progress_widget_metrics_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_progress_raster_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_progress_raster_host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_progress_raster_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_progress_raster_host:RasterTextClipped

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    "$@" --entry progress_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry progress_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/progress_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

# A bundle with one missing declared capability must fail before UI effects.
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --bind font_metrics:MeasureGlyphWidth=progress_widget_metrics_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=progress_widget_metrics_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_progress_raster_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_progress_raster_host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_progress_raster_host:RasterText \
    --entry progress_widget_portable_test:main -o "$work/missing.zib" "$portable"
if "$ziran" run "$work/missing.zib" > "$work/missing.log" 2>&1; then
    echo 'progress bundle ran without its clipped-text capability' >&2
    exit 1
fi
rg -q 'missing host capability: raster_text:RasterTextClipped' "$work/missing.log"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "progress_widget_behavior.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "progress_widget_behavior.h"
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
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/progress_widget_behavior.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --root "$module_root" \
            --module-path "$module_dir" --module-path "$ziran_root/std" -o "$output" "$module"
        if test "$target" = c; then
            cp "$work/native_main.h" "$output/main.c"
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cp "$work/native_main.h" "$output/main.cpp"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$output" \
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
    if ProgressWidgetBehavior_Answer() != 42 || host.measures != 2 || host.draws != 5 {
        t.Fatal("progress composition")
    }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
