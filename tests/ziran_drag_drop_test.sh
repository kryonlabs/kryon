#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/drag_drop.zi" "$work/drag_drop.zi"
cat > "$work/use_drag_drop.zi" <<'EOF'
#import "drag_drop"

#program_export
Answer :: () -> s32 {
    source: DragDropSourceDecision = DragDropSourceDecisionFor(
        false, 0, 7, false, false, true, 6, 8, true, true,
        true, true, false)
    if !source.valid || !source.start_source || !source.returns_active ||
        source.clear_source { return 0 }
    target: DragDropTargetDecision = DragDropTargetDecisionFor(
        false, false, true, true, true, true, true, 6, 4)
    if !target.accepted || target.copy_size != 4 ||
        !target.clear_source || !target.consume_release { return 0 }
    blocked: DragDropTargetDecision = DragDropTargetDecisionFor(
        true, false, true, true, true, true, true, 6, 4)
    if blocked.accepted || blocked.consume_release { return 0 }
    if !DragDropShouldClearSource(true, 7, 7, false, false) { return 0 }
    if DragDropCopySize(-3, 4) != 0 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/drag_drop.zi" "$work/use_drag_drop.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/drag_drop.zi" "$work/use_drag_drop.zi"
"$ziran" bundle --root "$work" --entry use_drag_drop:Answer \
    -o "$work/source.zib" "$work/drag_drop.zi" "$work/use_drag_drop.zi"
"$ziran" bundle --root "$work" --entry use_drag_drop:Answer \
    -o "$work/ir.zib" "$work/ir/drag_drop.zir" \
    "$work/ir/use_drag_drop.zir"
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
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                -o "$output" "$input_dir/drag_drop.$extension" \
                "$input_dir/use_drag_drop.$extension"
            if rg -q 'github.com/waozixyz/kryon' "$output"; then
                echo 'drag and drop imported the old Kryon Go runtime' >&2
                exit 1
            fi
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseDragDrop_Answer() != 42 { panic("wrong result") } }
GO
            GO111MODULE=off go run "$output/drag_drop.go" \
                "$output/use_drag_drop.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" -o "$output" \
                "$input_dir/drag_drop.$extension" \
                "$input_dir/use_drag_drop.$extension"
            cat > "$output/main.c" <<'C'
#include "use_drag_drop.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/drag_drop.c" "$output/use_drag_drop.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" -o "$output" \
                "$input_dir/drag_drop.$extension" \
                "$input_dir/use_drag_drop.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_drag_drop.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/drag_drop.cpp" "$output/use_drag_drop.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
