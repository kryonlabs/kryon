#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/geometry.zi" "$work/geometry.zi"
cp "$repo/src/ui/math.zi" "$work/math.zi"
cp "$repo/src/ui/input_props.zi" "$work/input_props.zi"
cp "$repo/src/ui/input.zi" "$work/input.zi"
cat > "$work/use_input.zi" <<'EOF'
#import "input"
#import "input_props"
#import "geometry"

#program_export
Answer :: () -> s32 {
    if InputDefaultStepButtonWidth(2.0) != 48 { return 0 }
    if !InputPointerDragShouldStart(7, 0, 5) { return 0 }
    if !InputPointerDragIsHorizontal(3, 2) { return 0 }
    pointer: InputPointerInteraction = InputPointerInteractionFor(
        true, false, false, true, true, false, true)
    if !pointer.active || !pointer.hovered || !pointer.activated ||
        !pointer.consume_release { return 0 }
    if !InputTempEditActivationFor(true, false, true, true, true,
        0.2, 1.0, 1.0, 6) { return 0 }
    bounds: Rectangle
    bounds.x = 10.0
    bounds.width = 100.0
    bounds.height = 30.0
    cell: InputCellLayout = InputCellLayoutFor(bounds, 2, 1, 10, true)
    if cell.cell.x != 60.0 || cell.field.width != 30.0 ||
        cell.minus.x != 90.0 || !cell.has_step_buttons { return 0 }
    if InputRoundValueForKind(cast(NumericValueKind)NumericInt, -1.6) != -2.0 { return 0 }
    discrete: InputStep = InputStepValueForKind(cast(NumericValueKind)NumericInt, 2.0,
        1.0, 3.0, 1, true)
    if !discrete.changed || discrete.value != 5.0 { return 0 }
    continuous: InputStep = InputStepValueForKind(cast(NumericValueKind)NumericDouble, 2.0,
        0.5, 2.0, 1, false)
    if !continuous.changed || continuous.value != 2.5 { return 0 }
    if FormatAnswer() != 42 { return 0 }
    return 42
}

#program_export
FormatAnswer :: () -> s32 {
    if InputDefaultFormat(cast(NumericValueKind)NumericInt) != "%d" { return 0 }
    if InputDefaultFormat(cast(NumericValueKind)NumericDouble) != "%.6f" { return 0 }
    if InputDefaultFormat(cast(NumericValueKind)NumericFloat) != "%.3f" { return 0 }
    return 42
}
EOF

sources="$work/geometry.zi $work/input_props.zi $work/input.zi $work/use_input.zi"
"$ziran" check --root "$work" $sources
"$ziran" ir --root "$work" -o "$work/ir" $sources
"$ziran" bundle --root "$work" --entry use_input:Answer \
    -o "$work/source.zib" $sources
"$ziran" bundle --root "$work" --entry use_input:Answer \
    -o "$work/ir.zib" "$work/ir/geometry.zir" \
    "$work/ir/input_props.zir" "$work/ir/input.zir" \
    "$work/ir/use_input.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$input_dir/geometry.$extension" \
                "$input_dir/input_props.$extension" \
                "$input_dir/input.$extension" \
                "$input_dir/use_input.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() {
    if UseInput_Answer() != 42 || UseInput_FormatAnswer() != 42 {
        panic("wrong input result")
    }
}
GO
            GO111MODULE=off go run "$output/geometry.go" "$output/math.go" \
                "$output/input_props.go" "$output/input.go" \
                "$output/use_input.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" -o "$output" \
                "$input_dir/geometry.$extension" \
                "$input_dir/input_props.$extension" \
                "$input_dir/input.$extension" \
                "$input_dir/use_input.$extension"
            cat > "$output/main.c" <<'C'
#include "use_input.h"
int main(void) { return Answer() == 42 && FormatAnswer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/geometry.c" "$output/math.c" "$output/input_props.c" \
                "$output/input.c" "$output/use_input.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" -o "$output" \
                "$input_dir/geometry.$extension" \
                "$input_dir/input_props.$extension" \
                "$input_dir/input.$extension" \
                "$input_dir/use_input.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_input.hpp"
int main() { return Answer() == 42 && FormatAnswer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/geometry.cpp" "$output/math.cpp" "$output/input_props.cpp" \
                "$output/input.cpp" "$output/use_input.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
