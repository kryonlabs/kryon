#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/use_swipe.zi" <<'EOF'
#module "use_swipe"
#import "swipe"
#import "swipe_props"
#import "geometry"

Answer :: () -> i32 #export {
    if SwipeDirectionsFor((u32)0) != (u32)SwipeAll { return 0 }
    if SwipeMinDistanceFor(2.0, 0.0) != 96.0 { return 0 }
    if SwipeAxisBiasFor(0.0) != 1.25 { return 0 }
    bounds: Rectangle
    bounds.width = 100.0
    bounds.height = 20.0
    if !SwipeCanBegin(bounds, true, true) { return 0 }
    delta: Vector2
    delta.x = -80.0
    delta.y = 5.0
    direction: SwipeDirection = SwipeDirectionFor(delta, (u32)SwipeAll, 1.25)
    if direction != (SwipeDirection)SwipeLeft { return 0 }
    if !SwipeShouldCancelForAxis(delta, (u32)SwipeVertical, 1.25) {
        return 0
    }
    drag: SwipeDragState = SwipeDragStateFor(delta, (u32)SwipeAll,
        1.25, 8.0, 48.0, false)
    if !drag.dragging || drag.cancelled ||
        drag.direction != (SwipeDirection)SwipeLeft ||
        drag.progress != 1.0 { return 0 }
    lifecycle: SwipeDragLifecycle = SwipeDragLifecycleFor(true, false,
        false, drag.dragging)
    if !lifecycle.claim_pointer_owner || !lifecycle.capture_input {
        return 0
    }
    release: SwipeReleaseState = SwipeReleaseStateFor(delta,
        (u32)SwipeAll, 1.25, 48.0, 0.2, 0.5, true)
    if !release.committed ||
        release.direction != (SwipeDirection)SwipeLeft { return 0 }
    slow: SwipeReleaseState = SwipeReleaseStateFor(delta,
        (u32)SwipeAll, 1.25, 48.0, 0.8, 0.5, true)
    if slow.committed || slow.direction != (SwipeDirection)SwipeNone {
        return 0
    }
    if !SwipeReleaseLifecycleFor(true).consume_release { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" --module-path "$repo/zi" \
    "$work/use_swipe.zi"
"$ziran" ir --root "$work" --module-path "$repo/zi" -o "$work/ir" \
    "$work/use_swipe.zi"
"$ziran" bundle --root "$work" --module-path "$repo/zi" \
    --entry use_swipe:Answer -o "$work/source.zib" "$work/use_swipe.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry use_swipe:Answer -o "$work/ir.zib" "$work/ir/use_swipe.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        input_dir=$work
        module_dir=$repo/zi
        extension=zi
    else
        input_dir=$work/ir
        module_dir=$work/ir
        extension=zir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_swipe.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseSwipe_Answer() != 42 { panic("wrong swipe result") } }
GO
            GO111MODULE=off go run "$output/geometry.go" \
                "$output/swipe_props.go" "$output/swipe.go" \
                "$output/use_swipe.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_swipe.$extension"
            cat > "$output/main.c" <<'C'
#include "use_swipe.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/geometry.c" "$output/swipe_props.c" \
                "$output/swipe.c" "$output/use_swipe.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/use_swipe.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_swipe.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/geometry.cpp" "$output/swipe_props.cpp" \
                "$output/swipe.cpp" "$output/use_swipe.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
