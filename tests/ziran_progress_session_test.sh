#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "control_props"
#import "drawing_props"
#import "geometry"
#import "paint_queue"
#import "progress"
#import "progress_props"
#import "progress_widget"
#import "style"
#import "style_sheet"
#import "tree"
#import "tree_draw"
#import "widget_kind"

frame_index :: i32 #global
progress_identity :: i32 #global

Frame :: () -> i32 #export {
    if frame_index == 1 {
        if !EndTree() || TreeCount() != 2 ||
            TreeNodeAt(1).kind != WidgetKindProgress { return -1 }
        progress_identity = TreeNodeAt(1).identity_generation
        if progress_identity <= 0 { return -1 }
        frame_index = 2
        return 1
    }
    if frame_index == 0 {
        rules: StyleRules
        rules.count = 2
        track: StyleRule
        track.selector = StyleDefaultSelector()
        track.selector.kind = StyleKindProgress()
        track.selector.role = ProgressTrackRole()
        track.selector.tone = (i32)ButtonToneNeutral
        track.style.fields = (u32)StyleBackground
        track.style.background = (u32)0x11223344
        rules.items[0] = track
        fill: StyleRule
        fill.selector = StyleDefaultSelector()
        fill.selector.kind = StyleKindProgress()
        fill.selector.role = ProgressFillRole()
        fill.selector.tone = (i32)ButtonToneAccent
        fill.style.fields = (u32)StyleBackground
        fill.style.background = (u32)0x55667788
        rules.items[1] = fill
        InstallStyleRules(rules)
    }
    props: ProgressProps
    props.bounds = (Rectangle){10.0, 20.0, 100.0, 20.0}
    props.min = 0
    props.max = 100
    props.value = 25
    props.label = "25%"
    props.key = (u64)17
    BeginTree((u64)10, (Rectangle){0.0, 0.0, 200.0, 100.0})
    Progress(props)
    if frame_index == 4 {
        color: Color = (Color){1, 2, 3, 4}
        index: i32 = 0
        while index < 4093 {
            PaintLine(1, props.bounds, color)
            index += 1
        }
        if EndTree() || TreeCount() != 2 ||
            TreeNodeAt(1).identity_generation != progress_identity {
            return -1
        }
        frame_index = 5
        return 4
    }
    if frame_index == 3 {
        index: i32 = 0
        while index < 1023 {
            TreeSubmit((u64)(index + 100), 0, WidgetKindBox,
                props.bounds)
            index += 1
        }
        if EndTree() || TreeCount() != 2 ||
            TreeNodeAt(1).identity_generation != progress_identity {
            return -1
        }
        frame_index = 4
        return 3
    }
    if frame_index == 0 {
        frame_index = 1
        return 0
    }
    if !EndTree() || TreeCount() != 2 ||
        TreeNodeAt(1).kind != WidgetKindProgress ||
        TreeNodeAt(1).identity_generation != progress_identity {
        return -1
    }
    frame_index = 3
    return 2
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Frame -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Frame -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_progress_session_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"
