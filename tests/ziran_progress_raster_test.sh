#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "progress"
#import "progress_raster"

Answer :: () -> i32 #export {
    bounds: Rectangle = (Rectangle){10.0, 20.0, 100.0, 20.0}
    paint: ProgressPaint
    paint.layout.fill_bounds = (Rectangle){10.0, 20.0, 25.0, 20.0}
    paint.layout.label_x = 40.9
    paint.layout.label_y = 25.9
    paint.track_color = (u32)0x11223344
    paint.fill_color = (u32)0x55667788
    paint.border_color = (u32)0x99aabbcc
    paint.label_color = (u32)0x10203080
    paint.filled_label_color = (u32)0x405060c0
    paint.radius = 5.0
    paint.border_width = 2.0
    PaintProgress(bounds, paint, "50%", 14, 0.5)
    paint.layout.fill_bounds.width = 0.0
    paint.border_width = 0.0
    PaintProgress(bounds, paint, "", 14, 1.0)
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

"${CC:-cc}" -std=c11 -I"$repo/include" -I"$repo/../ziran/include" \
    "$repo/tests/ziran_progress_raster_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$repo/../ziran/build/libziran.a" \
    -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#include <cstring>
#define CHECK assert
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#include <string.h>
#define CHECK assert
#define HOST
#endif
static int calls;
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    CHECK(radius == 0.25f && segments == 12);
    if(calls == 0 || calls == 4) {
        CHECK(bounds.x == 10 && bounds.y == 20 &&
              bounds.width == 100 && bounds.height == 20);
        CHECK(color.r == 0x11 && color.g == 0x22 &&
              color.b == 0x33 && color.a == 0x44);
    } else {
        CHECK(calls == 1 && bounds.width == 25);
        CHECK(color.r == 0x55 && color.g == 0x66 &&
              color.b == 0x77 && color.a == 0x88);
    }
    calls++;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    CHECK(calls == 2 && bounds.x == 10 && bounds.y == 20 &&
          bounds.width == 100 && bounds.height == 20);
    CHECK(radius == 0.25f && segments == 12 && width == 2.0f);
    CHECK(color.r == 0x99 && color.g == 0xaa &&
          color.b == 0xbb && color.a == 0xcc);
    calls++;
}
HOST void RasterText(String value, int32_t x, int32_t y, int32_t font,
    Color color) {
    CHECK(calls == 3 && value.length == 3 &&
          memcmp(value.data, "50%", 3) == 0);
    CHECK(x == 40 && y == 25 && font == 14);
    CHECK(color.r == 0x10 && color.g == 0x20 &&
          color.b == 0x30 && color.a == 0x40);
    calls++;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    CHECK(0 && "Progress should not draw clipped text");
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
    CHECK(0 && "Progress should not draw lines");
}
HOST void RasterImage(String path, uint32_t texture_id,
    Rectangle source, Rectangle destination, Rectangle clip,
    Vector2 origin, float rotation, float radius, Color tint) {
    (void)path; (void)texture_id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius; (void)tint;
    CHECK(0 && "Progress should not draw images");
}
int main(void) {
    CHECK(Answer() == 42 && calls == 5);
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
            cat > "$output/progress_test.go" <<'GO'
package ziran
import "testing"
type progressHost struct { t *testing.T; calls int }
func (host *progressHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if radius != 0.25 || segments != 12 { host.t.Fatal("radius") }
    if host.calls == 0 || host.calls == 4 {
        if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 100 ||
           bounds.Height != 20 || color.R != 0x11 || color.G != 0x22 ||
           color.B != 0x33 || color.A != 0x44 { host.t.Fatal("track") }
    } else if host.calls != 1 || bounds.Width != 25 ||
              color.R != 0x55 || color.G != 0x66 ||
              color.B != 0x77 || color.A != 0x88 { host.t.Fatal("fill") }
    host.calls++
}
func (host *progressHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if host.calls != 2 || bounds.X != 10 || bounds.Y != 20 ||
       bounds.Width != 100 || bounds.Height != 20 || radius != 0.25 ||
       segments != 12 || width != 2 || color.R != 0x99 ||
       color.G != 0xaa || color.B != 0xbb || color.A != 0xcc {
        host.t.Fatal("outline")
    }
    host.calls++
}
func (host *progressHost) RasterText(value string, x int32, y int32,
    font int32, color Color) {
    if host.calls != 3 || value != "50%" || x != 40 || y != 25 ||
       font != 14 || color.R != 0x10 || color.G != 0x20 ||
       color.B != 0x30 || color.A != 0x40 { host.t.Fatal("label") }
    host.calls++
}
func (host *progressHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    host.t.Fatal("Progress should not draw clipped text")
}
func (host *progressHost) RasterLine(line Rectangle, color Color) {
    host.t.Fatal("Progress should not draw lines")
}
func (host *progressHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    host.t.Fatal("Progress should not draw images")
}
func TestProgressRaster(t *testing.T) {
    host := &progressHost{t: t}
    SetRasterShapeHost(host)
    SetRasterTextHost(host)
    SetRasterHost(host)
    SetPaintQueueHost(host)
    if App_Answer() != 42 || host.calls != 5 { t.Fatal("draw order") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
