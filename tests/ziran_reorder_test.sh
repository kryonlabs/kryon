#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/use_reorder.zi" <<'EOF'
#import "control_props"
#import "geometry"
#import "reorder"
#import "reorder_props"
#import "style"

#program_export
Answer :: () -> s32 {
    handle: StyleFrame
    handle.value.fields = cast(u32)StyleContentOffset
    handle.value.offset_x = 36.0
    handle.value.offset_y = 5.0
    placeholder: StyleFrame
    placeholder.value.fields = cast(u32)StyleContentOffset
    placeholder.value.offset_x = 34.0
    placeholder.value.offset_y = 12.0
    metrics: ReorderMetrics = ReorderMetricsFor(2.0, 0, 0, 0, 0,
        handle, placeholder)
    if metrics.handle_width != 72 || metrics.drag_threshold != 10 ||
       metrics.auto_scroll_margin != 68 || metrics.auto_scroll_step != 24 {
        return 0
    }
    bounds: Rectangle = Rectangle.{10.0, 20.0, 200.0, 80.0}
    grip: Rectangle = ReorderHandleBounds(bounds, 36, 40)
    if grip.x != 10.0 || grip.width != 36.0 || grip.height != 40.0 {
        return 0
    }
    if ReorderTargetIndexFor(-2, 4) != 0 ||
       ReorderTargetIndexFor(9, 4) != 3 ||
       ReorderTargetIndexFor(0, 0) != -1 { return 0 }
    result: ReorderListResult = ReorderListActiveResultFor(1, 3, 4,
        99, 120, 75)
    result = ReorderListCommitResultFor(result, 6)
    if result.committed != 1 || result.from_index != 3 ||
       result.to_index != 4 || result.dragging != 0 { return 0 }
    if !ReorderPressFor(9, true, 3, true, false, true, true).can_press {
        return 0
    }
    if ReorderPressFor(9, true, 3, true, true, true, true).can_press {
        return 0
    }
    release: ReorderReleaseDecision = ReorderReleaseFor(true, true)
    if !release.capture_input || !release.commit ||
       !release.cancel_active { return 0 }
    metrics.drag_threshold = 10
    metrics.auto_scroll_margin = 20
    metrics.auto_scroll_step = 7
    motion: ReorderDragMotion = ReorderDragMotionFor(45, 100, 1,
        Rectangle.{10.0, 40.0, 200.0, 100.0}, 0, 0, 30, 80, metrics)
    if motion.dragging != 1 || motion.scroll_offset != 23 { return 0 }
    return 42
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/use_reorder.zi"
for input in source ir; do
    if test "$input" = source; then
        module=$work/use_reorder.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/use_reorder.zir
        module_dir=$work/ir
    fi
    "$ziran" bundle --root "$work" --module-path "$module_dir" \
        --entry use_reorder:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output="$work/$target-$input"
        "$ziran" build --target="$target" --strict --root "$work" \
            --module-path "$module_dir" -o "$output" "$module"
        if test "$target" = go; then
            cat > "$output/reorder_test.go" <<'GO'
package ziran
import "testing"
func TestReorder(t *testing.T) { if UseReorder_Answer() != 42 { t.Fatal("reorder result") } }
GO
            GO111MODULE=off go test "$output"/*.go
        elif test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "use_reorder.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cat > "$output/main.cpp" <<'CPP'
#include "use_reorder.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/ir.zib"
