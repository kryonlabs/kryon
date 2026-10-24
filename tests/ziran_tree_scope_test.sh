#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cp "$repo/tests/widget_test_session.zi" "$work/"

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "widget_test_session"

#import "geometry"
#import "tree"
#import "widget_kind"

using WidgetKind;

Frame :: (bounds: Rectangle) -> bool {
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreeCurrentParent(TestSession()) != 0 ||
        TreeCurrentParentKey(TestSession()) != cast(u64)9 { return false }
    outer: s32 = TreeSubmitCurrent(TestSession(), cast(u64)10, WidgetKindColumn, bounds)
    if outer != 1 || !TreePushScope(TestSession(), outer) ||
        TreeCurrentParent(TestSession()) != outer ||
        TreeCurrentParentKey(TestSession()) != cast(u64)10 { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)11, WidgetKindButton, bounds) != 2 {
        return false
    }
    inner: s32 = TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindGroup, bounds)
    if inner != 3 || !TreePushScope(TestSession(), inner) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)13, WidgetKindText, bounds) != 4 ||
        !TreePopScope(TestSession()) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)14, WidgetKindImage, bounds) != 5 ||
        !TreePopScope(TestSession()) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)15, WidgetKindRouter, bounds) != 6 {
        return false
    }
    return TreeFinish(TestSession())
}

#program_export
Answer :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 100.0, 80.0}
    if !Frame(bounds) || TreeCount(TestSession()) != 7 ||
        TreeNodeAt(TestSession(), 1).parent != 0 ||
        TreeNodeAt(TestSession(), 2).parent != 1 ||
        TreeNodeAt(TestSession(), 3).parent != 1 ||
        TreeNodeAt(TestSession(), 4).parent != 3 ||
        TreeNodeAt(TestSession(), 5).parent != 1 ||
        TreeNodeAt(TestSession(), 6).parent != 0 ||
        TreeNodeAt(TestSession(), 4).parent_key != cast(u64)12 ||
        TreeNodeAt(TestSession(), 1).first_child != 2 ||
        TreeNodeAt(TestSession(), 2).next_sibling != 3 ||
        TreeNodeAt(TestSession(), 3).next_sibling != 5 { return -1 }
    generation: u64 = TreeNodeAt(TestSession(), 4).identity_generation
    if !Frame(bounds) || TreeNodeAt(TestSession(), 4).identity_generation != generation {
        return -2
    }
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreePopScope(TestSession()) || TreeFinish(TestSession()) ||
        TreeCount(TestSession()) != 7 { return -5 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreePushScope(TestSession(), 99) || TreeFinish(TestSession()) ||
        TreeCount(TestSession()) != 7 { return -6 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    inner: s32 = TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindGroup, bounds)
    if !TreePushScope(TestSession(), inner) || TreeFinish(TestSession()) || TreeCount(TestSession()) != 7 ||
        TreeNodeAt(TestSession(), 4).identity_generation != generation { return -3 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    TreeCancel(TestSession())
    if TreeCurrentParent(TestSession()) != -1 || TreeSubmitCurrent(TestSession(), cast(u64)20,
        WidgetKindText, bounds) != -1 { return -4 }
    return 42
}
ZI

answer_go_file=scope_test.go
cat > "$work/answer_test.go" <<'GO'
package ziran
import "testing"
func TestScope(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("tree scope") }
}
GO
. "$repo/tests/answer_matrix.sh"
