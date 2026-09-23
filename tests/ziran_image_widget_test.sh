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
#import "drawing_props"
#import "geometry"
#import "image"
#import "image_props"
#import "image_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"

phase :: i32 #global

Frame :: () -> i32 #export {
    if phase == 1 {
        if !EndTree() || TreeCount() != 2 ||
            TreeNodeAt(1).semantic_label != "Hero image" { return -1 }
        phase = 2
        return 1
    }
    image: ImageProps
    image.class_name = 7
    image.key = (u64)17
    if phase == 0 {
        rules: StyleRules
        rules.count = 1
        tint: StyleRule
        tint.selector = StyleDefaultSelector()
        tint.selector.kind = StyleKindImage()
        tint.selector.class_name = 7
        tint.style.fields = (u32)StyleForeground |
            (u32)StyleOpacity | (u32)StyleRadius
        tint.style.foreground = (u32)0x102030ff
        tint.style.opacity = 0.5
        tint.style.radius = 6.0
        rules.items[0] = tint
        InstallStyleRules(rules)
        image.asset_path = "assets/hero.png"
        image.alt_text = "Hero image"
        image.bounds = (Rectangle){10.0, 20.0, 100.0, 100.0}
        image.fit = (ImageFit)ImageFitContain
        image.rotation = 15.0
        BeginTree((u64)10, (Rectangle){0.0, 0.0, 200.0, 200.0})
        Image(image)
        phase = 1
        return 0
    }
    if phase == 2 {
        image.asset_path = "ignored.png"
        image.bounds = (Rectangle){20.0, 30.0, 60.0, 40.0}
        image.fit = (ImageFit)ImageFitCover
        image.texture.id = (u32)77
        image.texture.width = 40
        image.texture.height = 40
        Image(image)
        phase = 3
        return 2
    }
    image.asset_path = "missing.png"
    image.alt_text = "Missing hero"
    image.bounds = (Rectangle){0.0, 0.0, 100.0, 20.0}
    BeginTree((u64)10, (Rectangle){0.0, 0.0, 200.0, 200.0})
    Image(image)
    if !EndTree() || TreeNodeAt(1).semantic_label != "Missing hero" {
        return -1
    }
    phase = 4
    return 3
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
    "$repo/tests/ziran_image_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native.zi" <<'ZI'
#module "native"
#import "drawing_props"
#import "geometry"
#import "image_props"
#import "image_widget"

Answer :: () -> i32 #export {
    image: ImageProps
    image.asset_path = "ignored.png"
    image.bounds = (Rectangle){20.0, 30.0, 60.0, 40.0}
    image.fit = (ImageFit)ImageFitCover
    image.texture.id = (u32)77
    image.texture.width = 40
    image.texture.height = 40
    Image(image)
    return 42
}
ZI

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "native.hpp"
#include <cassert>
#define CHECK assert
#define HOST extern "C"
#else
#include "native.h"
#include <assert.h>
#define CHECK assert
#define HOST
#endif
static int draws;
HOST int32_t ImageWidth(String path) {
    (void)path;
    CHECK(0 && "texture-backed Image should not load an asset");
    return 0;
}
HOST int32_t ImageHeight(String path) {
    (void)path;
    CHECK(0 && "texture-backed Image should not load an asset");
    return 0;
}
HOST void RasterImage(String path, uint32_t texture_id,
    Rectangle source, Rectangle destination, Rectangle clip,
    Vector2 origin, float rotation, float radius, Color tint) {
    CHECK(path.length == 11 && texture_id == 77);
    CHECK(source.width == 40 && source.height == 40);
    CHECK(destination.x == 20 && destination.y == 20 &&
          destination.width == 60 && destination.height == 60);
    CHECK(clip.x == 20 && clip.y == 30 &&
          clip.width == 60 && clip.height == 40);
    CHECK(origin.x == 0 && origin.y == 0 && rotation == 0 && radius == 0);
    CHECK(tint.r == 255 && tint.g == 255 &&
          tint.b == 255 && tint.a == 255);
    draws++;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color; CHECK(0 && "unexpected line");
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
    CHECK(0 && "unexpected shape");
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
    int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width; (void)color;
    CHECK(0 && "unexpected outline");
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    CHECK(0 && "unexpected text");
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color; (void)clip;
    CHECK(0 && "unexpected clipped text");
}
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String typeface) {
    (void)value; (void)font; (void)typeface;
    CHECK(0 && "unexpected glyph measurement");
    return 0;
}
HOST int32_t MeasureGlyphLineHeight(int32_t font, String typeface) {
    (void)font; (void)typeface;
    CHECK(0 && "unexpected line height measurement");
    return 0;
}
int main(void) { CHECK(Answer() == 42 && draws == 1); return 0; }
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
        cat > "$output/image_widget_test.go" <<'GO'
package ziran
import "testing"
type imageHost struct { t *testing.T; draws int }
func (h *imageHost) ImageWidth(path string) int32 {
    h.t.Fatal("unexpected asset width"); return 0
}
func (h *imageHost) ImageHeight(path string) int32 {
    h.t.Fatal("unexpected asset height"); return 0
}
func (h *imageHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    if path != "ignored.png" || id != 77 ||
       source.Width != 40 || source.Height != 40 ||
       destination.X != 20 || destination.Y != 20 ||
       destination.Width != 60 || destination.Height != 60 ||
       clip.X != 20 || clip.Y != 30 ||
       clip.Width != 60 || clip.Height != 40 ||
       origin.X != 0 || origin.Y != 0 || rotation != 0 || radius != 0 ||
       tint.R != 255 || tint.G != 255 ||
       tint.B != 255 || tint.A != 255 { h.t.Fatal("image draw") }
    h.draws++
}
func TestImageWidget(t *testing.T) {
    host := &imageHost{t: t}
    SetImageRasterHost(host)
    SetPaintQueueHost(host)
    if Native_Answer() != 42 || host.draws != 1 { t.Fatal("image") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
