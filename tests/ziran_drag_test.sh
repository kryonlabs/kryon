#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/use_drag.zi" <<'EOF'
#module "use_drag"
#import "geometry"
#import "drag"

Answer :: () -> i32 #export {
    if DragComponentTokenFor(5, 2) != 83 { return 0 }
    if DragTextInsetFor(2.0) != 12.0 { return 0 }
    bounds: Rectangle
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.width = 100.0
    bounds.height = 30.0
    cell: Rectangle = DragCellBoundsFor(bounds, 2, 1)
    if cell.x != 60.0 || cell.width != 50.0 { return 0 }
    paint: DragTextPaint = DragCellTextPaintFor(cell, 5.0, 10.0)
    if paint.text_x != 65.0 || paint.text_y != 30.0 { return 0 }
    pointer: DragPointerDecision = DragPointerDecisionFor(false, false,
        true, false, false, true, true, false)
    if !pointer.start_active || !pointer.update_delta ||
        pointer.finish_active { return 0 }
    input: DragKeyboardInput = DragKeyboardInputFor(1, false, false,
        false, true)
    step: DragStep = DragKeyboardValue(10.0, 2.0, 0.0, 100.0, input)
    if !step.changed || step.value != 30.0 { return 0 }
    discrete: DragDiscreteStep = DragDiscreteKeyboardValue(10, 2.0,
        0, 100, input)
    if !discrete.changed || discrete.value != 30 { return 0 }
    delta: DragStep = DragDeltaValue(10.0, 3.0, 2.0, 0.0, 100.0)
    if !delta.changed || delta.value != 16.0 { return 0 }
    discrete_delta: DragDiscreteStep = DragDiscreteDeltaValue(10, 1.5,
        2.0, 0, 100)
    if !discrete_delta.changed || discrete_delta.value != 13 { return 0 }
    if DragClamp(100.0, 0.0, 20.0) != 20.0 { return 0 }
    if !DragKeyboardShouldRun(true, true, false) { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" --module-path "$repo/zi" "$work/use_drag.zi"
"$ziran" ir --root "$work" --module-path "$repo/zi" -o "$work/ir" \
    "$work/use_drag.zi"
"$ziran" bundle --root "$work" --entry use_drag:Answer \
    --module-path "$repo/zi" -o "$work/source.zib" "$work/use_drag.zi"
"$ziran" bundle --root "$work" --entry use_drag:Answer \
    --module-path "$work/ir" -o "$work/ir.zib" "$work/ir/use_drag.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
        module_dir=$repo/zi
    else
        extension=zir
        input_dir=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_drag.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseDrag_Answer() != 42 { panic("wrong drag result") } }
GO
            GO111MODULE=off go run "$output/geometry.go" \
                "$output/drag.go" "$output/use_drag.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" -o "$output" \
                --module-path "$module_dir" "$input_dir/use_drag.$extension"
            cat > "$output/main.c" <<'C'
#include "use_drag.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/geometry.c" "$output/drag.c" \
                "$output/use_drag.c" "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_drag.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_drag.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/geometry.cpp" "$output/drag.cpp" \
                "$output/use_drag.cpp" "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
