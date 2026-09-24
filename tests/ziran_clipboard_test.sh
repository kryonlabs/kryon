#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "clipboard"

Answer :: () -> i32 #export {
    state: ClipboardState
    state = ClipboardObserve(state, "external")
    if ClipboardRead(state, (ClipboardSource)ClipboardSystem) != "external" ||
        ClipboardHasText(state, (ClipboardSource)ClipboardPrimary) { return -1 }
    state = ClipboardSetPrimary(state, "selected")
    if ClipboardRead(state, (ClipboardSource)ClipboardPrimaryOrSystem) != "selected" {
        return -2
    }
    state = ClipboardCopySelection(state, "copy")
    write: ClipboardWrite = ClipboardPendingWrite(state)
    if !write.available || write.text != "copy" ||
        ClipboardRead(state, (ClipboardSource)ClipboardPrimary) != "copy" { return -3 }
    state = ClipboardObserve(state, "stale")
    if ClipboardRead(state, (ClipboardSource)ClipboardSystem) != "copy" { return -4 }
    state = ClipboardWriteCompleted(state)
    state = ClipboardObserve(state, "fresh")
    if ClipboardPendingWrite(state).available ||
        ClipboardRead(state, (ClipboardSource)ClipboardSystem) != "fresh" { return -5 }
    state = ClipboardCopySelection(state, "")
    if ClipboardRead(state, (ClipboardSource)ClipboardPrimaryOrSystem) != "fresh" ||
        ClipboardPendingWrite(state).available { return -6 }
    state = ClipboardRequestWrite(state, "")
    if !ClipboardPendingWrite(state).available ||
        ClipboardHasText(state, (ClipboardSource)ClipboardSystem) { return -7 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/host.c" <<'C'
#include "ziran_host.h"
#include <assert.h>
int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    long long answer = 0;
    int has_answer = 0;
    assert(BundleRun(bundle, NULL, 0, &answer, &has_answer));
    assert(has_answer && answer == 42);
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
    "$work/host.c" "$ziran_lib" \
    -o "$work/host"
"$work/host" "$work/source.zib"
"$work/host" "$work/saved.zib"
