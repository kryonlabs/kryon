#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#module "app"
#import "drawing_props"
#import "fieldset"
#import "fieldset_props"
#import "geometry"
#import "link"
#import "link_props"
#import "progress"
#import "progress_props"
#import "radio"
#import "radio_props"
#import "router_props"
#import "separator"
#import "separator_props"
#import "style"
#import "text"
#import "text_props"
#import "toast"
#import "toast_props"

Answer :: () -> i32 #export {
    link: LinkProps
    link.text = "Read more"
    link.link = "https://example.org"
    link.bounds.width = 0.0
    if link.text.length != 9 || link.link != "https://example.org" ||
        LinkBoundsFor(link.bounds, 70, 12, 14).width != 70.0 {
        return 0
    }

    toast: ToastProps
    toast.message = "Saved"
    metrics: ToastMetrics
    metrics.default_seconds = 3.0
    request: ToastRequestDecision = ToastRequestDecisionFor(
        toast.message.length > 0, (float)toast.seconds, metrics)
    if !request.show || request.clear || request.seconds != 3.0 {
        return 0
    }

    frame: StyleFrame
    frame.value.background = (u32)0x11223344
    separator: SeparatorProps
    separator.label = "Section"
    separator.bounds.width = 100.0
    line: SeparatorLine = SeparatorLineFor(separator.bounds,
        separator.vertical, frame)
    if separator.label != "Section" || line.color != (u32)0x11223344 {
        return 0
    }

    radio: RadioProps
    radio.id = 7
    radio.label = "Choice"
    if radio.label.length != 6 ||
        RadioActivationFor(radio.id, true, radio.disabled) != 7 { return 0 }

    progress: ProgressProps
    progress.label = "Downloads"
    progress.max = 100
    progress.value = 25
    if progress.label.length != 9 ||
        ProgressRatio(progress.min, progress.max, progress.value) != 0.25 {
        return 0
    }

    route: RouterRoute
    route.path = "/home"
    route.title = "Home"
    route.group = "primary"
    if route.path != "/home" || route.title != "Home" ||
        route.group != "primary" { return 0 }

    fieldset: FieldsetProps
    fieldset.title = "Details"
    fieldset.bounds.width = 100.0
    paint: FieldsetPaint = FieldsetPaintFor(fieldset.bounds, 40.0,
        fieldset.title.length > 0, 1.0, frame)
    if !paint.show_title || paint.title_text.width != 40.0 { return 0 }

    text: TextProps
    text.text = "one two"
    text.wrap = (TextWrap)TextWrapAuto
    element: TextElement
    element.text = text.text
    if element.text != "one two" ||
        TextWrapPolicy(text.bounds.width, (i32)text.wrap) != 1 { return 0 }
    return 42
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

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
            cat > "$output/main.c" <<'C'
#include "app.h"
void RasterLine(Rectangle line, Color color) { (void)line; (void)color; }
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
extern "C" void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
}
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/props_test.go" <<'GO'
package ziran
import "testing"
func TestPortableWidgetProps(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("portable widget props") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
