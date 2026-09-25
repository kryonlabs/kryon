#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#import "control_props"
#import "geometry"
#import "rows"
#import "style"

#program_export
Answer :: () -> s32 {
    frame: StyleFrame
    metrics: InfoRowsMetrics = InfoRowsMetricsFor(0, 0, 2.0, frame)
    if metrics.row_height != 64 || metrics.padding_x != 20 { return 0 }
    background: InfoRowsLayout = InfoRowsLayoutFor(5, 7, -3, 2, metrics)
    if background.background.width != 0.0 ||
       background.background.height != 128.0 { return 0 }
    text: InfoRowLayout = InfoRowLayoutFor(5, 7, 20, -1, metrics)
    if text.text_bounds.x != 25.0 || text.text_bounds.y != 7.0 ||
       text.text_bounds.width != 0.0 { return 0 }

    wrap: ButtonRowWrapDecision = ButtonRowWrapFor(70, 40, 100, 5)
    if !wrap.wraps || wrap.row_width != 40 { return 0 }
    placement: ButtonRowPlacement = ButtonRowPlacementFor(10, 100, 3, 5)
    if placement.start_x != 10 || placement.button_width != 30 ||
       placement.total_width != 100 { return 0 }
    if ButtonRowTotalHeight(3, 20, 5) != 70 { return 0 }
    next: FormRectResult = FormRectFor(3, 4, 10, -2, 6)
    if next.bounds.height != 0.0 || next.next_cursor_y != 10 { return 0 }
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
                "$input_dir/app.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("row layout") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/app.$extension"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/app.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
