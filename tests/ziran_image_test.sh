#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/use_image.zi" <<'EOF'
#import "drawing_props"
#import "image"
#import "image_props"
#import "geometry"

#program_export
Answer :: () -> s32 {
    bounds: Rectangle
    bounds.width = 100.0
    bounds.height = 50.0
    source: Rectangle
    source.width = 20.0
    source.height = 20.0
    contain: Rectangle = ImageFitBounds(bounds, source, 20, 20, 1)
    if contain.x != 25.0 || contain.y != 0.0 ||
        contain.width != 50.0 || contain.height != 50.0 { return 0 }
    image: ImageProps
    image.asset_path = "assets/photo.png"
    image.alt_text = "A photo"
    image.bounds = bounds
    image.source = source
    image.fit = cast(ImageFit)ImageFitContain
    texture: Texture2D
    texture.id = cast(u32)1
    texture.width = 20
    texture.height = 20
    fitted: Rectangle = ImageFitRect(image, texture)
    if image.asset_path != "assets/photo.png" ||
        image.alt_text != "A photo" ||
        fitted.x != contain.x || fitted.width != contain.width {
        return 0
    }
    tint: Color
    plan: ImageDrawPlan = ImageDrawPlanFor(image, texture, tint)
    if !plan.draw || plan.source.width != 20.0 ||
        plan.destination.x != 25.0 || plan.tint.r != cast(u8)255 ||
        plan.tint.a != cast(u8)255 { return 0 }
    image.source.width = 0.0
    plan = ImageDrawPlanFor(image, texture, tint)
    if plan.source.width != 20.0 || plan.source.height != 20.0 {
        return 0
    }
    texture.id = cast(u32)0
    plan = ImageDrawPlanFor(image, texture, tint)
    if plan.draw { return 0 }
    texture.id = cast(u32)1
    image.source = source
    image.bounds.width = 0.0
    if ImageDrawPlanFor(image, texture, tint).draw { return 0 }
    image.bounds = bounds
    cover: Rectangle = ImageFitBounds(bounds, source, 20, 20, 2)
    if cover.x != 0.0 || cover.y != -25.0 ||
        cover.width != 100.0 || cover.height != 100.0 { return 0 }
    stretch: Rectangle = ImageFitBounds(bounds, source, 20, 20, 0)
    if stretch.width != 100.0 || stretch.height != 50.0 { return 0 }
    source.width = 0.0
    source.height = 0.0
    inferred: Rectangle = ImageFitBounds(bounds, source, 20, 20, 1)
    if inferred.x != 25.0 || inferred.width != 50.0 { return 0 }
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.height = 40.0
    placeholder: ImagePlaceholderLayout = ImagePlaceholderLayoutFor(
        bounds, 20, 10)
    if placeholder.label_x != 50 || placeholder.label_y != 35 {
        return 0
    }
    if ImagePlaceholderFontFor(12, 0) != 12 ||
        ImagePlaceholderFontFor(12, 18) != 18 { return 0 }
    source.width = 200.0
    source.height = 100.0
    bounds.height = 50.0
    strip: Rectangle
    strip.x = 35.0
    strip.y = 30.0
    strip.width = 25.0
    strip.height = 10.0
    mapped: Rectangle = ImageStripSource(source, bounds, strip)
    if mapped.x != 50.0 || mapped.y != 20.0 ||
        mapped.width != 50.0 || mapped.height != 20.0 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" --module-path "$repo/src/ui" "$work/use_image.zi"
"$ziran" ir --root "$work" --module-path "$repo/src/ui" -o "$work/ir" \
    "$work/use_image.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry use_image:Answer -o "$work/source.zib" "$work/use_image.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry use_image:Answer -o "$work/ir.zib" "$work/ir/use_image.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        input_dir=$work
        module_dir=$repo/src/ui
        extension=zi
    else
        input_dir=$work/ir
        module_dir=$work/ir
        extension=zir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_image.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseImage_Answer() != 42 { panic("wrong image result") } }
GO
            GO111MODULE=off go run "$output/geometry.go" "$output/math.go" \
                "$output/text_align.go" "$output/drawing_props.go" \
                "$output/image_props.go" "$output/image.go" \
                "$output/use_image.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_image.$extension"
            cat > "$output/main.c" <<'C'
#include "use_image.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/geometry.c" "$output/math.c" "$output/text_align.c" \
                "$output/drawing_props.c" "$output/image_props.c" \
                "$output/image.c" \
                "$output/use_image.c" "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_image.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_image.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/geometry.cpp" "$output/math.cpp" "$output/text_align.cpp" \
                "$output/drawing_props.cpp" "$output/image_props.cpp" \
                "$output/image.cpp" \
                "$output/use_image.cpp" "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
