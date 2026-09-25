#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

#import "flow"
#import "geometry"
#import "tree"
#import "widget_kind"

using WidgetKind;

#program_export
Answer :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 200.0, 60.0}
    TreeStart(TestSession(), cast(u64)1, bounds)
    props: FlowProps
    props.key = cast(u64)10
    props.gap = 3
    props.padding = 5
    flow: FlowResult = Flow(TestSession(), props)
    if !flow.opened || flow.node != 1 ||
        flow.bounds.width != 200.0 || flow.content.x != 5.0 ||
        flow.content.width != 190.0 || flow.gap != 3 ||
        flow.padding != 5 { return -1 }
    empty: Rectangle
    first: Rectangle = FlowChildBounds(flow, empty,
        Rectangle.{0.0, 0.0, 20.0, 0.0}, 0, 0.0)
    second: Rectangle = FlowChildBounds(flow, empty,
        Rectangle.{0.0, 0.0, 30.0, 0.0}, 1, 20.0)
    if first.x != 5.0 || first.y != 5.0 ||
        first.width != 20.0 || first.height != 50.0 ||
        second.x != 28.0 || second.y != 5.0 ||
        second.width != 30.0 || second.height != 50.0 {
        return -2
    }
    if TreeSubmitCurrent(TestSession(), cast(u64)11, WidgetKindText, first) != 2 ||
        TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindImage, second) != 3 ||
        !End(TestSession()) || !TreeFinish(TestSession()) || TreeCount(TestSession()) != 4 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindRow ||
        TreeNodeAt(TestSession(), 2).parent != 1 || TreeNodeAt(TestSession(), 3).parent != 1 ||
        TreeNodeAt(TestSession(), 3).bounds.x != 28.0 { return -3 }
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
        "$ziran" build --target="$target" \
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
            cat > "$output/flow_test.go" <<'GO'
package ziran
import "testing"
func TestFlow(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("flow") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
