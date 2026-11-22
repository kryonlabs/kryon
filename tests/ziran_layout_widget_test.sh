#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cp "$repo/tests/widget_test_session.zi" "$work/"

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "widget_test_session"

#import "geometry"
#import "grid"
#import "grid_props"
#import "layout_behavior"
Layout :: #import "kryon/layout";
#import "style_sheet"
#import "control_props"
#import "tree"
#import "widget_kind"

using WidgetKind;
using StyleField;
using StyleFlexField;

AutoAnswer :: () -> s32 {
    root: Rectangle = Rectangle.{0.0, 0.0, 100.0, 80.0}
    TreeStart(TestSession(), cast(u64)101, root)
    props: Layout.Props
    props.key = cast(u64)102
    props.bounds = root
    props.gap = 3
    props.padding = 5
    column: Layout.LayoutContainer = Layout.Draw(TestSession(), props)
    child: Rectangle = Rectangle.{0.0, 0.0, 10.0, 12.0}
    first: s32 = TreeSubmitCurrent(TestSession(), cast(u64)103, WidgetKindText, child)
    second: s32 = TreeSubmitCurrent(TestSession(), cast(u64)104, WidgetKindText, child)
    if !column.opened || TreeSubmittedNodeAt(TestSession(), first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(TestSession(), first).bounds.y != 5.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.y != 20.0 || !End(TestSession()) {
        return -20
    }
    props.key = cast(u64)105
    row: Layout.LayoutContainer = Layout.Row(TestSession(), props)
    first = TreeSubmitCurrent(TestSession(), cast(u64)106, WidgetKindText, child)
    second = TreeSubmitCurrent(TestSession(), cast(u64)107, WidgetKindText, child)
    if !row.opened || TreeSubmittedNodeAt(TestSession(), first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.x != 18.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.y != 5.0 || !End(TestSession()) {
        return -21
    }
    props.key = cast(u64)108
    stack: Layout.LayoutContainer = Layout.Stack(TestSession(), props)
    first = TreeSubmitCurrent(TestSession(), cast(u64)109, WidgetKindText, child)
    second = TreeSubmitCurrent(TestSession(), cast(u64)110, WidgetKindText, child)
    if !stack.opened || TreeSubmittedNodeAt(TestSession(), first).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.x != 5.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.y != 5.0 || !End(TestSession()) {
        return -22
    }
    grid_props: GridProps
    grid_props.key = cast(u64)111
    grid_props.bounds = root
    grid_props.columns = 2
    grid_props.gap = 4
    grid_props.padding = 2
    grid: Layout.GridResult = Layout.Grid(TestSession(), grid_props)
    first = TreeSubmitCurrent(TestSession(), cast(u64)112, WidgetKindText, child)
    second = TreeSubmitCurrent(TestSession(), cast(u64)113, WidgetKindText, child)
    third: s32 = TreeSubmitCurrent(TestSession(), cast(u64)114, WidgetKindText, child)
    if !grid.container.opened ||
        TreeSubmittedNodeAt(TestSession(), first).bounds.x != 2.0 ||
        TreeSubmittedNodeAt(TestSession(), first).bounds.width != 46.0 ||
        TreeSubmittedNodeAt(TestSession(), second).bounds.x != 52.0 ||
        TreeSubmittedNodeAt(TestSession(), third).bounds.x != 2.0 ||
        TreeSubmittedNodeAt(TestSession(), third).bounds.y != 18.0 ||
        !End(TestSession()) || !TreeFinish(TestSession()) { return -23 }
    return 42
}

grid_measure_calls: s32;

MeasuredHeight :: (width: float32, key: u64) -> s32 {
    grid_measure_calls += cast(s32)key
    return cast(s32)(width / 2.0)
}

MeasuredAnswer :: () -> s32 {
    session := TestSession()
    root: Rectangle = .{0.0, 0.0, 200.0, 140.0}
    TreeStart(session, cast(u64)201, root)
    outer := Layout.Grid(session, GridProps.{.key = 202, .columns = 2, .padding = 2, .gap = 4,
        .bounds = .{10.0, 20.0, 100.0, 100.0}})
    grid_measure_calls = 0
    items: [2]GridItem = .[.{.measure_height = MeasuredHeight, .measure_key = 1}, .{.height = 12}]
    item_bounds: [2]Rectangle
    inner := Layout.Grid(session, GridProps.{.key = 203, .padding = 1, .gap = 3, .columns = 1}, items[:], item_bounds[:])
    if !inner.container.opened || inner.container.bounds.width != 46.0 || inner.container.bounds.height != 39.0 ||
        grid_measure_calls != 1 || item_bounds[0].x != 13.0 || item_bounds[0].y != 23.0 || item_bounds[0].width != 44.0 ||
        item_bounds[0].height != 22.0 || item_bounds[1].y != 48.0 { return -24 }
    child := TreeSubmitCurrent(session, cast(u64)204, WidgetKindText, item_bounds[1])
    if !End(session) || !End(session) || !TreeFinish(session) || TreeNodeAt(session, child).bounds.y != 48.0 ||
        TreeNodeAt(session, inner.container.node).parent != outer.container.node { return -25 }
    return 42
}

FlexAnswer :: () -> s32 {
    rules: StyleRules
    rules.count = 2
    selector := StyleDefaultSelector()
    selector.kind = StyleKindRow()
    selector.class_name = 11
    rules.items[0] = .{.selector = selector,
        .style = .{.fields = cast(u32)StyleGap | cast(u32)StyleRowGap, .gap = 7.0, .row_gap = 5.0},
        .flex = .{.fields = cast(u32)StyleFlexWrap | cast(u32)StyleFlexAlign, .wrap = true, .align_items = 1}}
    selector.kind = StyleKindText()
    selector.class_name = 12
    rules.items[1] = .{.selector = selector, .flex = .{.fields = cast(u32)StyleFlexGrow |
        cast(u32)StyleFlexBasis | cast(u32)StyleFlexSelf, .grow = 1.0, .basis = 20.0, .align_self = 2}}
    if !InstallStyleRules(rules) { return -26 }
    session := TestSession()
    TreeStart(session, cast(u64)301, Rectangle.{0.0, 0.0, 200.0, 140.0})
    items: [3]Layout.FlexItem = .[.{.kind = StyleKindText(), .class_name = 12, .width = 40.0, .height = 12.0},
        .{.width = 30.0, .height = 24.0}, .{.width = 80.0, .height = 20.0}]
    boxes: [3]Layout.FlexPlacement
    row := Layout.Row(session, Layout.Props.{.key = 302, .class_name = 11, .bounds = .{10.0, 20.0, 100.0, 0.0}},
        items[:], boxes[:])
    if !row.opened || row.bounds.height != 49.0 || boxes[0].bounds.x != 10.0 || boxes[0].bounds.y != 32.0 ||
        boxes[0].bounds.width != 63.0 || boxes[1].bounds.x != 80.0 || boxes[2].bounds.y != 49.0 { return -27 }
    child := TreeSubmitCurrent(session, cast(u64)303, WidgetKindText, boxes[2].bounds)
    rules.count = 0
    if !End(session) || !TreeFinish(session) || TreeNodeAt(session, child).bounds.y != 49.0 ||
        !InstallStyleRules(rules) { return -28 }
    return 42
}

Leaf :: (session: Session, key: s32, width: float32, height: float32) -> Rectangle {
    node := TreeSubmitCurrent(session, cast(u64)key, WidgetKindText, Rectangle.{0.0, 0.0, width, height})
    return TreeSubmittedBounds(session, node)
}

// Containers without a height size to the children they place.
ContentAnswer :: () -> s32 {
    session := TestSession()
    root: Rectangle = .{0.0, 0.0, 200.0, 300.0}
    TreeStart(session, cast(u64)401, root)
    outer := Layout.Draw(session, Layout.Props.{.key = 402, .bounds = root, .gap = 4, .gap_override = true,
        .padding_override = true})
    column := Layout.Draw(session, Layout.Props.{.key = 403, .gap = 2, .gap_override = true,
        .padding = 3, .padding_override = true})
    if !outer.opened || !column.opened || column.bounds.height != 0.0 { return -30 }
    unused Leaf(session, 404, 10.0, 12.0)
    unused Leaf(session, 405, 10.0, 8.0)
    row := Layout.Row(session, Layout.Props.{.key = 406, .gap = 5, .gap_override = true,
        .padding_override = true})
    unused Leaf(session, 407, 20.0, 10.0)
    right := Leaf(session, 408, 20.0, 16.0)
    if !row.opened || right.x != 28.0 || right.y != 27.0 || !End(session) ||
        TreeSubmittedBounds(session, row.node).height != 16.0 { return -31 }
    if Leaf(session, 409, 10.0, 6.0).y != 45.0 || !End(session) ||
        TreeSubmittedBounds(session, column.node).height != 54.0 { return -32 }
    if Leaf(session, 410, 10.0, 10.0).y != 58.0 { return -33 }
    grid := Layout.Grid(session, GridProps.{.key = 411, .columns = 2, .gap = 6, .padding_override = true})
    unused Leaf(session, 412, 0.0, 20.0)
    cell := Layout.Draw(session, Layout.Props.{.key = 413, .padding_override = true})
    if !grid.container.opened || !cell.opened || cell.bounds.x != 103.0 || cell.bounds.width != 97.0 { return -34 }
    unused Leaf(session, 414, 10.0, 30.0)
    if !End(session) { return -35 }
    Layout.GridSpan(session, grid.container, -1)
    wide := Leaf(session, 415, 0.0, 10.0)
    if wide.y != 108.0 || wide.width != 200.0 || !End(session) ||
        TreeSubmittedBounds(session, grid.container.node).height != 46.0 { return -36 }
    flow := Layout.Flow(session, Layout.Props.{.key = 416, .gap = 4, .gap_override = true,
        .padding_override = true})
    unused Leaf(session, 417, 120.0, 10.0)
    second := Leaf(session, 418, 70.0, 14.0)
    third := Leaf(session, 419, 90.0, 8.0)
    if !flow.opened || second.x != 124.0 || second.y != 122.0 || third.x != 0.0 || third.y != 140.0 ||
        !End(session) || TreeSubmittedBounds(session, flow.node).height != 26.0 { return -37 }
    if !End(session) || !TreeFinish(session) { return -38 }
    return 42
}

#program_export
Answer :: () -> s32 {
    root: Rectangle = Rectangle.{0.0, 0.0, 300.0, 200.0}
    TreeStart(TestSession(), cast(u64)1, root)
    props: Layout.Props
    props.key = cast(u64)10
    screen: Layout.LayoutContainer = Layout.Screen(TestSession(), props)
    if !screen.opened || screen.node != 1 ||
        screen.bounds.width != 300.0 ||
        screen.kind != WidgetKindGroup { return -1 }

    props.key = cast(u64)11
    props.gap = 4
    props.padding = 5
    column: Layout.LayoutContainer = Layout.Draw(TestSession(), props)
    if !column.opened || column.node != 2 ||
        column.content.width != 290.0 { return -2 }
    first := TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindText, Rectangle.{0.0, 0.0, 0.0, 12.0})
    placed := TreeSubmittedBounds(TestSession(), first)
    if first != 3 || placed.x != 5.0 || placed.y != 5.0 ||
        placed.width != 290.0 || placed.height != 12.0 { return -3 }
    props.key = cast(u64)13
    props.bounds = Rectangle.{5.0, 21.0, 200.0, 40.0}
    props.gap = 0
    props.padding = 0
    row: Layout.LayoutContainer = Layout.Row(TestSession(), props)
    if !row.opened || row.node != 4 ||
        row.kind != WidgetKindRow { return -5 }
    image := TreeSubmitCurrent(TestSession(), cast(u64)14, WidgetKindImage, Rectangle.{0.0, 0.0, 20.0, 0.0})
    placed = TreeSubmittedBounds(TestSession(), image)
    if image != 5 || placed.x != 5.0 || placed.y != 21.0 || placed.height != 40.0 ||
        !End(TestSession()) || !End(TestSession()) { return -6 }
    // Children at explicit coordinates stay out of the content size.
    if TreeSubmittedBounds(TestSession(), column.node).height != 22.0 { return -4 }

    grid_props: GridProps
    grid_props.key = cast(u64)15
    grid_props.bounds = Rectangle.{0.0, 0.0, 120.0, 60.0}
    grid_props.columns = 2
    grid_props.gap = 4
    grid_props.padding = 2
    grid: Layout.GridResult = Layout.Grid(TestSession(), grid_props)
    if !grid.container.opened || grid.container.node != 6 ||
        grid.cursor.metrics.columns != 2 ||
        grid.cursor.metrics.cell_width != 56 { return -7 }
    cursor: GridCursor = GridStep(grid.cursor, 10, 1)
    if cursor.item.x != 2.0 || cursor.item.y != 2.0 ||
        cursor.item.width != 56.0 ||
        TreeSubmitCurrent(TestSession(), cast(u64)16, WidgetKindText,
            cursor.item) != 7 { return -8 }
    cursor = GridStep(cursor, 15, 1)
    if cursor.item.x != 62.0 || cursor.item.y != 2.0 ||
        TreeSubmitCurrent(TestSession(), cast(u64)17, WidgetKindText,
            cursor.item) != 8 || !End(TestSession()) { return -9 }

    props.key = cast(u64)18
    props.bounds = Rectangle.{0.0, 0.0, 80.0, 60.0}
    props.gap = 0
    props.padding = 3
    stack: Layout.LayoutContainer = Layout.Stack(TestSession(), props)
    fill := TreeSubmitCurrent(TestSession(), cast(u64)19, WidgetKindBox, Rectangle.{})
    item := TreeSubmittedBounds(TestSession(), fill)
    if !stack.opened || stack.node != 9 || fill != 10 ||
        item.x != 3.0 || item.y != 3.0 ||
        item.width != 74.0 || item.height != 54.0 ||
        !End(TestSession()) { return -11 }
    props.key = cast(u64)20
    group: Layout.LayoutContainer = Layout.Group(TestSession(), props)
    if !group.opened || group.node != 11 || !End(TestSession()) ||
        !End(TestSession()) || !TreeFinish(TestSession()) || TreeCount(TestSession()) != 12 ||
        TreeNodeAt(TestSession(), 5).parent != 4 || TreeNodeAt(TestSession(), 8).parent != 6 ||
        TreeNodeAt(TestSession(), 11).parent != 1 { return -12 }
    auto := AutoAnswer()
    if auto != 42 { return auto }
    measured := MeasuredAnswer()
    if measured != 42 { return measured }
    flex := FlexAnswer()
    if flex != 42 { return flex }
    return ContentAnswer()
}
ZI

answer_go_file=layout_test.go
cat > "$work/answer_test.go" <<'GO'
package ziran
import "testing"
func TestLayout(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("layout") }
}
GO
. "$repo/tests/answer_matrix.sh"
