#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

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

frame_index: s32;
progress_identity: u64;

#program_export
Frame :: () -> s32 {
    if frame_index == 1 {
        if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).kind != WidgetKindProgress { return -1 }
        progress_identity = TreeNodeAt(TestSession(), 1).identity_generation
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
        track.selector.tone = cast(s32)ButtonToneNeutral
        track.style.fields = cast(u32)StyleBackground
        track.style.background = cast(u32)0x11223344
        rules.items[0] = track
        fill: StyleRule
        fill.selector = StyleDefaultSelector()
        fill.selector.kind = StyleKindProgress()
        fill.selector.role = ProgressFillRole()
        fill.selector.tone = cast(s32)ButtonToneAccent
        fill.style.fields = cast(u32)StyleBackground
        fill.style.background = cast(u32)0x55667788
        rules.items[1] = fill
        InstallStyleRules(rules)
    }
    props: ProgressProps
    props.bounds = Rectangle.{10.0, 20.0, 100.0, 20.0}
    props.min = 0
    props.max = 100
    props.value = 25
    props.label = "25%"
    props.key = cast(u64)17
    BeginFrame(TestSession(), cast(u64)10, Rectangle.{0.0, 0.0, 200.0, 100.0})
    Progress(TestSession(), props)
    if frame_index == 4 {
        color: Color = Color.{1, 2, 3, 4}
        index: s32 = 0
        while index < 4093 {
            PaintLine(TestSession(), 1, props.bounds, color)
            index += 1
        }
        if PaintOverflow(TestSession()) { return -1 }
        TreeCancel(TestSession())
        if EndFrame(TestSession()) == cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).identity_generation != progress_identity {
            return -1
        }
        frame_index = 5
        return 4
    }
    if frame_index == 3 {
        index: s32 = 0
        while index < 1023 {
            if TreeSubmit(TestSession(), cast(u64)(index + 100), 0, WidgetKindBox,
                props.bounds) < 0 { return -1 }
            index += 1
        }
        TreeCancel(TestSession())
        if EndFrame(TestSession()) == cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
            TreeNodeAt(TestSession(), 1).identity_generation != progress_identity {
            return -1
        }
        frame_index = 4
        return 3
    }
    if frame_index == 0 {
        frame_index = 1
        return 0
    }
    if EndFrame(TestSession()) != cast(FrameStatus)FrameOk || TreeCount(TestSession()) != 2 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindProgress ||
        TreeNodeAt(TestSession(), 1).identity_generation != progress_identity {
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_progress_session_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"
