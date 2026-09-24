#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "scroll"
#import "scroll_props"
#import "scroll_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

Answer :: () -> i32 #export {
    root: Rectangle = (Rectangle){0.0, 0.0, 200.0, 100.0}
    TreeStart((u64)1, root)
    props: ScrollProps
    props.key = (u64)2
    props.bounds = (Rectangle){20.0, 10.0, 80.0, 40.0}
    props.content_height = 140
    props.scroll_offset = 30
    props.scroll_delta = 20
    result: ScrollResult = Scroll(props)
    if !result.opened || result.node != 1 ||
        result.max_scroll != 100 || result.scroll_offset != 50 ||
        result.viewport.y != 10.0 || result.content.y != -40.0 ||
        result.content.height != 140.0 { return -1 }
    child: Rectangle = (Rectangle){result.content.x - 10.0,
        result.content.y + 45.0, 40.0, 20.0}
    node: i32 = TreeSubmitCurrent((u64)3, WidgetKindButton, child)
    TreeSetInteractive(node, false, false, 3)
    if node != 2 || !End() || !TreeFinish() { return -2 }
    if TreeNodeAt(node).clip.x != 20.0 ||
        TreeNodeAt(node).clip.y != 10.0 ||
        TreeHitAt(15.0, 15.0) != -1 ||
        TreeHitAt(25.0, 15.0) != node { return -3 }

    TreeStart((u64)1, root)
    props.scroll_offset = 2147483647
    result = Scroll(props)
    if result.scroll_offset != 100 || !End() ||
        !TreeFinish() { return -4 }
    TreeStart((u64)1, root)
    props.scroll_offset = -2147483647
    props.scroll_delta = -20
    result = Scroll(props)
    if result.scroll_offset != 0 || !End() ||
        !TreeFinish() { return -5 }

    TreeStart((u64)1, root)
    props.scroll_offset = 20
    props.scroll_delta = 0
    props.input.enabled = true
    props.input.pointer_allowed = true
    props.input.wheel = -1.0
    result = Scroll(props)
    if result.scroll_offset != 62 || !result.frame.consume_wheel ||
        !result.frame.scrollbar || result.frame.clip.width != 70.0 ||
        result.content.y != -52.0 || !End() || !TreeFinish() { return -6 }

    TreeStart((u64)1, root)
    props.scroll_offset = result.scroll_offset
    props.input.wheel = 0.0
    props.input.pressed = true
    props.input.down = true
    props.input.mouse = (Vector2){95.0, 20.0}
    result = Scroll(props)
    if !result.frame.start_drag || result.frame.clear_drag ||
        !End() || !TreeFinish() { return -7 }

    TreeStart((u64)1, root)
    props.input.pressed = false
    props.input.owns_drag = true
    props.input.grab = result.frame.grab
    props.input.mouse.y = 45.0
    result = Scroll(props)
    if result.scroll_offset <= 62 || result.scroll_offset > 100 ||
        !End() || !TreeFinish() { return -8 }

    TreeStart((u64)1, root)
    props.input.down = false
    props.input.released = true
    result = Scroll(props)
    if !result.frame.clear_drag || !result.frame.consume_release ||
        !End() || !TreeFinish() { return -9 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/app.zi
        root=$work
        module_path=$repo/src/ui
    else
        source=$work/ir/app.zir
        root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$module_path" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict \
            --root "$root" --module-path "$module_path" \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/scroll_test.go" <<'GO'
package ziran
import "testing"
func TestScroll(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("scroll") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
