#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "grid"
#import "grid_props"
#import "layout"
#import "layout_widget"
#import "tree"
#import "widget_kind"

AutoAnswer :: () -> i32 {
    root: Rectangle = (Rectangle){0.0, 0.0, 100.0, 80.0}
    TreeStart((u64)101, root)
    props: ColumnProps
    props.key = (u64)102
    props.bounds = root
    props.gap = 3
    props.padding = 5
    column: LayoutContainer = Column(props)
    child: Rectangle = (Rectangle){0.0, 0.0, 10.0, 12.0}
    first: i32 = TreeSubmitCurrent((u64)103, WidgetKindText, child)
    second: i32 = TreeSubmitCurrent((u64)104, WidgetKindText, child)
    if !column.opened || TreeSubmittedNodeAt(first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(first).bounds.y != 5.0 ||
        TreeSubmittedNodeAt(second).bounds.y != 20.0 || !End() {
        return -20
    }
    props.key = (u64)105
    row: LayoutContainer = Row(props)
    first = TreeSubmitCurrent((u64)106, WidgetKindText, child)
    second = TreeSubmitCurrent((u64)107, WidgetKindText, child)
    if !row.opened || TreeSubmittedNodeAt(first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(second).bounds.x != 18.0 ||
        TreeSubmittedNodeAt(second).bounds.y != 5.0 || !End() {
        return -21
    }
    props.key = (u64)108
    stack: LayoutContainer = Stack(props)
    first = TreeSubmitCurrent((u64)109, WidgetKindText, child)
    second = TreeSubmitCurrent((u64)110, WidgetKindText, child)
    if !stack.opened || TreeSubmittedNodeAt(first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(second).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(second).bounds.y != 5.0 || !End() {
        return -22
    }
    grid_props: GridProps
    grid_props.key = (u64)111
    grid_props.bounds = root
    grid_props.columns = 2
    grid_props.gap = 4
    grid_props.padding = 2
    grid: GridResult = Grid(grid_props)
    first = TreeSubmitCurrent((u64)112, WidgetKindText, child)
    second = TreeSubmitCurrent((u64)113, WidgetKindText, child)
    third: i32 = TreeSubmitCurrent((u64)114, WidgetKindText, child)
    if !grid.container.opened ||
        TreeSubmittedNodeAt(first).bounds.x != 2.0 ||
        TreeSubmittedNodeAt(first).bounds.width != 46.0 ||
        TreeSubmittedNodeAt(second).bounds.x != 52.0 ||
        TreeSubmittedNodeAt(third).bounds.x != 2.0 ||
        TreeSubmittedNodeAt(third).bounds.y != 18.0 ||
        !End() || !TreeFinish() { return -23 }
    return 42
}

Answer :: () -> i32 #export {
    root: Rectangle = (Rectangle){0.0, 0.0, 300.0, 200.0}
    TreeStart((u64)1, root)
    props: ColumnProps
    props.key = (u64)10
    screen: LayoutContainer = Screen(props)
    if !screen.opened || screen.node != 1 ||
        screen.bounds.width != 300.0 ||
        screen.kind != WidgetKindGroup { return -1 }

    props.key = (u64)11
    props.gap = 4
    props.padding = 5
    column: LayoutContainer = Column(props)
    if !column.opened || column.node != 2 ||
        column.content.width != 290.0 { return -2 }
    empty: Rectangle
    first: Rectangle = ColumnChildBounds(column, empty,
        (Rectangle){0.0, 0.0, 0.0, 12.0}, 0, 0.0)
    second: Rectangle = ColumnChildBounds(column, empty,
        (Rectangle){0.0, 0.0, 20.0, 20.0}, 1, 12.0)
    if first.x != 5.0 || first.y != 5.0 ||
        first.width != 290.0 || first.height != 12.0 ||
        second.x != 5.0 || second.y != 21.0 ||
        second.width != 20.0 { return -3 }
    if TreeSubmitCurrent((u64)12, WidgetKindText, first) != 3 {
        return -4
    }
    props.key = (u64)13
    props.bounds = (Rectangle){5.0, 21.0, 200.0, 40.0}
    props.gap = 0
    props.padding = 0
    row: LayoutContainer = Row(props)
    if !row.opened || row.node != 4 ||
        row.kind != WidgetKindRow { return -5 }
    item: Rectangle = RowChildBounds(row, empty,
        (Rectangle){0.0, 0.0, 20.0, 0.0}, 0, 0.0)
    if item.x != 5.0 || item.y != 21.0 ||
        item.height != 40.0 ||
        TreeSubmitCurrent((u64)14, WidgetKindImage, item) != 5 ||
        !End() || !End() { return -6 }

    grid_props: GridProps
    grid_props.key = (u64)15
    grid_props.bounds = (Rectangle){0.0, 0.0, 120.0, 60.0}
    grid_props.columns = 2
    grid_props.gap = 4
    grid_props.padding = 2
    grid: GridResult = Grid(grid_props)
    if !grid.container.opened || grid.container.node != 6 ||
        grid.cursor.metrics.columns != 2 ||
        grid.cursor.metrics.cell_width != 56 { return -7 }
    cursor: GridCursor = GridStep(grid.cursor, 10, 1)
    if cursor.item.x != 2.0 || cursor.item.y != 2.0 ||
        cursor.item.width != 56.0 ||
        TreeSubmitCurrent((u64)16, WidgetKindText,
            cursor.item) != 7 { return -8 }
    cursor = GridStep(cursor, 15, 1)
    if cursor.item.x != 62.0 || cursor.item.y != 2.0 ||
        TreeSubmitCurrent((u64)17, WidgetKindText,
            cursor.item) != 8 || !End() { return -9 }

    props.key = (u64)18
    props.bounds = (Rectangle){0.0, 0.0, 80.0, 60.0}
    props.gap = 0
    props.padding = 3
    stack: LayoutContainer = Stack(props)
    item = StackChildBoundsFor(stack, empty, empty)
    if !stack.opened || stack.node != 9 ||
        item.x != 3.0 || item.y != 3.0 ||
        item.width != 74.0 || item.height != 54.0 ||
        TreeSubmitCurrent((u64)19, WidgetKindBox, item) != 10 ||
        !End() { return -11 }
    props.key = (u64)20
    group: LayoutContainer = Group(props)
    if !group.opened || group.node != 11 || !End() ||
        !End() || !TreeFinish() || TreeCount() != 12 ||
        TreeNodeAt(5).parent != 4 || TreeNodeAt(8).parent != 6 ||
        TreeNodeAt(11).parent != 1 { return -12 }
    return AutoAnswer()
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
            cat > "$output/layout_test.go" <<'GO'
package ziran
import "testing"
func TestLayout(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("layout") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
