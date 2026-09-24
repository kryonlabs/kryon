#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "control_props"
#import "geometry"
#import "style"
#import "style_sheet"
#import "text_align"
#import "text_props"
#import "text_widget"
#import "tree"
#import "tree_draw"
#import "widget_kind"

phase: s32;

#program_export
Frame :: () -> s32 {
    if phase == 1 {
        if !EndTree() || TreeCount() != 2 { return -1 }
        node: TreeEntry = TreeNodeAt(1)
        if node.kind != WidgetKindText || node.key != cast(u64)7 ||
            node.bounds.width != 50.0 || node.bounds.height != 24.0 ||
            node.semantic_label != "Alpha beta\nGamma" { return -2 }
        phase = 2
        return 42
    }
    rules: StyleRules
    rules.count = 1
    rule: StyleRule
    rule.selector = StyleDefaultSelector()
    rule.selector.kind = StyleKindText()
    rule.selector.class_name = 7
    rule.style.fields = cast(u32)StyleForeground |
        cast(u32)StyleFontSize | cast(u32)StyleOpacity
    rule.style.foreground = cast(u32)0x112233ff
    rule.style.font_size = 14.0
    rule.style.opacity = 0.5
    rules.items[0] = rule
    InstallStyleRules(rules)
    props: TextProps
    props.key = cast(u64)7
    props.bounds = Rectangle.{10.0, 20.0, 50.0, 24.0}
    props.text = "Alpha beta\nGamma"
    props.class_name = 7
    props.wrap = cast(TextWrap)TextWrapAuto
    props.align = cast(TextAlign)TextAlignCenter
    props.strikethrough = true
    BeginTree(cast(u64)1, Rectangle.{0.0, 0.0, 200.0, 100.0})
    Text(props)
    phase = 1
    return 0
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
    "$repo/tests/ziran_text_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"
