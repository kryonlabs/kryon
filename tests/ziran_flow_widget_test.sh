#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cp "$repo/tests/widget_test_session.zi" "$work/"

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "widget_test_session"

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

answer_go_file=flow_test.go
cat > "$work/answer_test.go" <<'GO'
package ziran
import "testing"
func TestFlow(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("flow") }
}
GO
. "$repo/tests/answer_matrix.sh"
