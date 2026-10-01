#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "clipboard"

using ClipboardSource;

#program_export
Answer :: () -> s32 {
    state: ClipboardState
    state = ClipboardObserve(state, "external")
    if ClipboardRead(state, cast(ClipboardSource)ClipboardSystem).text != "external" ||
        ClipboardHasText(state, cast(ClipboardSource)ClipboardPrimary) { return -1 }
    state = ClipboardSetPrimary(state, "selected")
    if ClipboardRead(state, cast(ClipboardSource)ClipboardPrimaryOrSystem).text != "selected" {
        return -2
    }
    state = ClipboardCopySelection(state, "copy")
    write: ClipboardWrite = ClipboardPendingWrite(state)
    if !write.available || write.text != "copy" ||
        ClipboardRead(state, cast(ClipboardSource)ClipboardPrimary).text != "copy" { return -3 }
    state = ClipboardObserve(state, "stale")
    if ClipboardRead(state, cast(ClipboardSource)ClipboardSystem).text != "copy" { return -4 }
    state = ClipboardWriteCompleted(state)
    state = ClipboardObserve(state, "fresh")
    if ClipboardPendingWrite(state).available ||
        ClipboardRead(state, cast(ClipboardSource)ClipboardSystem).text != "fresh" { return -5 }
    state = ClipboardCopySelection(state, "")
    if ClipboardRead(state, cast(ClipboardSource)ClipboardPrimaryOrSystem).text != "fresh" ||
        ClipboardPendingWrite(state).available { return -6 }
    state = ClipboardRequestWrite(state, "")
    if !ClipboardPendingWrite(state).available ||
        ClipboardHasText(state, cast(ClipboardSource)ClipboardSystem) { return -7 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42
