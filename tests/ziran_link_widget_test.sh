#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "accessibility_policy"
#import "accessibility_props"
#import "geometry"
#import "link_props"
#import "link_widget"
#import "semantic"
#import "tree"
#import "tree_draw"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global

Frame :: () -> i32 #export {
    props: LinkProps
    props.key = (u64)7
    props.focus_id = 7
    props.bounds = (Rectangle){10.0, 20.0, 0.0, 0.0}
    props.text = "Docs"
    props.link = "https://example.test/docs"
    props.focus_activate = phase == 2 || phase == 3
    props.disabled = phase == 3
    BeginTree((u64)1, (Rectangle){0.0, 0.0, 100.0, 60.0})
    result: LinkResult = Link(props)
    if !EndTree() || TreeCount() != 2 || result.node != 1 ||
        TreeNodeAt(1).kind != WidgetKindLink ||
        TreeNodeAt(1).semantic_kind != (SemanticKind)SemanticLink ||
        TreeNodeAt(1).semantic_label != "Docs" ||
        TreeNodeAt(1).bounds.width != 40.0 ||
        TreeNodeAt(1).bounds.height != 12.0 { return -10 }
    if phase == 0 {
        if result.activated || result.open_url || result.url != "" ||
            AccessibilityActionsFor(WidgetKindLink, 7,
                false, false, false) == (u32)0 { return -1 }
        TreePointerUpdate((PointerFrame){15.0, 25.0,
            true, true, false})
        TreePointerUpdate((PointerFrame){15.0, 25.0,
            false, false, true})
    } else if phase == 1 {
        if !result.activated || !result.open_url ||
            result.url != "https://example.test/docs" { return -2 }
        TreePointerUpdate((PointerFrame){0.0, 0.0,
            false, false, false})
    } else if phase == 2 {
        if !result.activated || !result.open_url { return -3 }
    } else {
        if result.activated || result.open_url ||
            TreeHitAt(15.0, 25.0) != -1 { return -4 }
    }
    old: i32 = phase
    phase += 1
    return old
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Frame -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Frame -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_link_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"
