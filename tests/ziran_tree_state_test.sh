#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "tree"
#import "widget_kind"

Check :: () -> i32 #export {
    bounds: Rectangle = (Rectangle){0.0, 0.0, 100.0, 20.0}
    TreeStart((u64)10, bounds)
    if TreeSubmit((u64)17, 0, WidgetKindProgress, bounds) != 1 {
        return 0
    }
    if !TreeFinish() || TreeCount() != 2 { return 0 }
    first: i32 = TreeNodeAt(1).identity_generation
    if first <= 0 { return 0 }

    bounds.width = 200.0
    TreeStart((u64)10, bounds)
    if TreeSubmit((u64)17, 0, WidgetKindProgress, bounds) != 1 {
        return 0
    }
    if !TreeFinish() || TreeNodeAt(1).identity_generation != first {
        return 0
    }
    if TreeNodeAt(1).bounds.width != 200.0 { return 0 }

    TreeStart((u64)10, bounds)
    TreeSubmit((u64)18, 0, WidgetKindProgress, bounds)
    if !TreeFinish() || TreeNodeAt(1).identity_generation == first {
        return 0
    }

    TreeStart((u64)10, bounds)
    TreeSubmit((u64)17, 0, WidgetKindProgress, bounds)
    TreeSubmit((u64)17, 0, WidgetKindProgress, bounds)
    if !TreeFinish() || TreeCount() != 3 { return 0 }
    if TreeNodeAt(1).identity_generation ==
        TreeNodeAt(2).identity_generation { return 0 }

    kept: i32 = TreeNodeAt(1).identity_generation
    TreeStart((u64)10, bounds)
    index: i32 = 0
    while index < 1023 {
        if TreeSubmit((u64)(index + 100), 0,
            WidgetKindProgress, bounds) < 0 { return 0 }
        index += 1
    }
    if TreeSubmit((u64)9999, 0, WidgetKindProgress, bounds) >= 0 {
        return 0
    }
    if TreeFinish() || TreeCount() != 3 ||
        TreeNodeAt(1).identity_generation != kept { return 0 }
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
