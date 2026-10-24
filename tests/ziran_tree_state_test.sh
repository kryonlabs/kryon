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

#program_export
Check :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 100.0, 20.0}
    TreeStart(TestSession(), cast(u64)10, bounds)
    if TreeSubmit(TestSession(), cast(u64)17, 0, WidgetKindProgress, bounds) != 1 {
        return 0
    }
    if !TreeFinish(TestSession()) || TreeCount(TestSession()) != 2 { return 0 }
    if TreeNodeAt(TestSession(), 0).first_child != 1 ||
        TreeNodeAt(TestSession(), 1).parent != 0 { return 0 }
    first: u64 = TreeNodeAt(TestSession(), 1).identity_generation
    if first <= 0 { return 0 }

    bounds.width = 200.0
    TreeStart(TestSession(), cast(u64)10, bounds)
    if TreeSubmit(TestSession(), cast(u64)17, 0, WidgetKindProgress, bounds) != 1 {
        return 0
    }
    if !TreeFinish(TestSession()) || TreeNodeAt(TestSession(), 1).identity_generation != first {
        return 0
    }
    if TreeNodeAt(TestSession(), 1).bounds.width != 200.0 { return 0 }

    TreeStart(TestSession(), cast(u64)10, bounds)
    TreeSubmit(TestSession(), cast(u64)18, 0, WidgetKindProgress, bounds)
    if !TreeFinish(TestSession()) || TreeNodeAt(TestSession(), 1).identity_generation == first {
        return 0
    }

    TreeStart(TestSession(), cast(u64)10, bounds)
    TreeSubmit(TestSession(), cast(u64)17, 0, WidgetKindProgress, bounds)
    if TreeSubmit(TestSession(), cast(u64)17, 0,
        WidgetKindProgress, bounds) >= 0 || TreeFinish(TestSession()) ||
        TreeCount(TestSession()) != 2 { return 0 }

    TreeStart(TestSession(), cast(u64)10, bounds)
    index: s32 = 0
    while index < 1100 {
        if TreeSubmit(TestSession(), cast(u64)(index + 100), 0,
            WidgetKindProgress, bounds) < 0 { return 0 }
        index += 1
    }
    if TreeSubmit(TestSession(), cast(u64)9999, 0, WidgetKindProgress, bounds) < 0 {
        return 0
    }
    if !TreeFinish(TestSession()) || TreeCount(TestSession()) != 1102 { return 0 }
    kept: u64 = TreeNodeAt(TestSession(), 1).identity_generation
    TreeStart(TestSession(), cast(u64)10, bounds)
    TreeSubmit(TestSession(), cast(u64)20, 0, WidgetKindProgress, bounds)
    TreeCancel(TestSession())
    if TreeCount(TestSession()) != 1102 ||
        TreeNodeAt(TestSession(), 1).identity_generation != kept { return 0 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry app:Check -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Check -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
