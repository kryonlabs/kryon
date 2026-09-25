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

#import "geometry"
#import "tree"
#import "widget_kind"

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

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Check -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Check -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
