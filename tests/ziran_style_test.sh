#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/use_style.zi" <<'EOF'
#import "control_props"
#import "surface"
#import "style"
#import "separator"
#import "progress"
#import "geometry"

using ButtonState;
using StyleField;

#program_export
Answer :: () -> s32 {
    if (DefaultFields() & cast(u32)StyleBackground) == cast(u32)0 { return 0 }
    if ResolveState(ButtonStateAuto, false, false, false,
        true, false, false) != ButtonStateHover { return 0 }
    if ResolveState(ButtonStateHover, true, false, false,
        true, false, false) != ButtonStateDisabled { return 0 }
    base: StyleData
    base.background = cast(u32)99
    base.foreground = cast(u32)11
    base.typeface = "base"
    override: StyleData
    override.fields = cast(u32)StyleBackground | cast(u32)StyleTypeface
    override.background = cast(u32)0
    override.typeface = "new"
    merged: StyleData = MergeValues(base, override)
    if merged.background != cast(u32)0 || merged.foreground != cast(u32)11 ||
        merged.typeface != "new" { return 0 }
    states: StyleStates
    states.normal.fields = cast(u32)StyleForeground
    states.normal.foreground = cast(u32)22
    states.hover.fields = cast(u32)StyleRadius
    states.hover.radius = 4.0
    active: StyleData = ResolveValues(base, states, ButtonStateHover)
    if active.foreground != cast(u32)22 || active.radius != 4.0 { return 0 }
    endpoint: FillStates = FillState(cast(u32)StyleBackgroundEnd,
        cast(u32)3, cast(u32)7)
    empty: FillStates
    fill: FillStates = FillTransition(endpoint, endpoint, empty, empty,
        0.5, 0.25, 0.0)
    if !fill.normal || !fill.hover || fill.press ||
        fill.normal_end != cast(u32)7 || fill.hover_amount != 0.5 { return 0 }
    frame: StyleFrame
    frame.value = active
    frame.fill = fill
    if frame.value.radius != 4.0 || !frame.fill.normal { return 0 }
    frame.value.fields = cast(u32)StyleGap
    frame.value.gap = 10.0
    frame.value.background = cast(u32)7
    frame.value.foreground = cast(u32)8
    bounds: Rectangle
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.width = 100.0
    bounds.height = 20.0
    line: SeparatorLine = SeparatorLineFor(bounds, false, frame)
    if line.line.y != 30.0 || line.line.width != 100.0 ||
        line.color != cast(u32)7 { return 0 }
    vertical: SeparatorLine = SeparatorLineFor(bounds, true, frame)
    if vertical.line.x != 60.0 || vertical.line.height != 20.0 {
        return 0
    }
    label: SeparatorLabelPaint = SeparatorLabelPaintFor(bounds,
        20.0, true, 14, 1.0, frame)
    if label.text.y != 23.0 || label.line.x != 40.0 ||
        label.line.width != 70.0 || !label.show_text ||
        !label.show_line { return 0 }
    bullet: BulletPaint = BulletPaintFor(bounds, frame)
    if bullet.radius != 5.0 || bullet.center.x != 60.0 ||
        bullet.center.y != 30.0 || bullet.color != cast(u32)8 { return 0 }
    progress: ProgressLayout = ProgressLayoutFor(bounds, 0, 100, 25,
        20.0, 10.0, 5.0)
    if progress.ratio != 0.25 || progress.fill_bounds.width != 25.0 ||
        progress.label_x != 40.0 || progress.label_y != 25.0 ||
        progress.label_on_fill { return 0 }
    if ProgressDrawRadius(bounds, 5.0) != 0.25 { return 0 }
    track: StyleFrame = frame
    track.value.border = cast(u32)9
    track.value.border_width = 2.0
    track.value.radius = 5.0
    filled: StyleFrame = frame
    filled.value.background = cast(u32)10
    painted: ProgressPaint = ProgressPaintFor(bounds, 0, 100, 25,
        20.0, 10.0, 1.0, track, filled, frame)
    if painted.track_color != cast(u32)7 ||
        painted.fill_color != cast(u32)10 ||
        painted.border_color != cast(u32)9 ||
        painted.border_width != 2.0 ||
        painted.layout.fill_bounds.width != 25.0 { return 0 }
    painted.label_color = cast(u32)0x11223380
    painted.filled_label_color = cast(u32)0x445566c0
    plan: ProgressDrawPlan = ProgressDrawPlanFor(bounds, painted, true, 0.5)
    if plan.radius != 0.25 || !plan.draw_fill || !plan.draw_border ||
        !plan.draw_label || plan.label_color != cast(u32)0x11223340 {
        return 0
    }
    painted.layout.label_on_fill = true
    plan = ProgressDrawPlanFor(bounds, painted, false, 0.5)
    if plan.draw_label || plan.label_color != cast(u32)0x44556660 { return 0 }
    painted.layout.fill_bounds.width = 0.0
    painted.border_width = 0.0
    plan = ProgressDrawPlanFor(bounds, painted, false, 1.0)
    if plan.draw_fill || plan.draw_border { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" --module-path "$repo/src/ui" "$work/use_style.zi"
"$ziran" ir --root "$work" --module-path "$repo/src/ui" -o "$work/ir" \
    "$work/use_style.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry use_style:Answer -o "$work/source.zib" "$work/use_style.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry use_style:Answer -o "$work/ir.zib" "$work/ir/use_style.zir"
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
                "$input_dir/use_style.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseStyle_Answer() != 42 { panic("wrong style result") } }
GO
            GO111MODULE=off go run "$output/control_props.go" \
                "$output/surface.go" "$output/style.go" \
                "$output/geometry.go" "$output/math.go" "$output/text_align.go" \
                "$output/drawing_props.go" \
                "$output/raster.go" "$output/separator.go" \
                "$output/progress.go" \
                "$output/use_style.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_style.$extension"
            cat > "$output/main.c" <<'C'
#include "use_style.h"
#include "raster.h"
void RasterLine(Rectangle line, Color color) { (void)line; (void)color; }
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/control_props.c" "$output/surface.c" \
                "$output/style.c" "$output/geometry.c" "$output/math.c" \
                "$output/drawing_props.c" "$output/raster.c" \
                "$output/separator.c" "$output/progress.c" \
                "$output/use_style.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_style.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_style.hpp"
#include "raster.hpp"
extern "C" void RasterLine(Rectangle line, Color color) { (void)line; (void)color; }
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/control_props.cpp" "$output/surface.cpp" \
                "$output/style.cpp" "$output/geometry.cpp" "$output/math.cpp" \
                "$output/drawing_props.cpp" "$output/raster.cpp" \
                "$output/separator.cpp" "$output/progress.cpp" \
                "$output/use_style.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
