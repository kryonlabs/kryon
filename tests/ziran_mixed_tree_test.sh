#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "progress_props"
#import "progress_widget"
#import "separator_props"
#import "separator_widget"
#import "tree"
#import "tree_draw"

Frame :: () -> i32 #export {
    progress: ProgressProps
    progress.bounds = (Rectangle){10.0, 20.0, 100.0, 20.0}
    progress.min = 0
    progress.max = 100
    progress.value = 50
    separator: SeparatorProps
    separator.bounds = (Rectangle){20.0, 50.0, 20.0, 40.0}
    separator.vertical = true
    BeginTree((u64)10, (Rectangle){0.0, 0.0, 200.0, 200.0})
    Progress(progress)
    Separator(separator)
    progress.bounds.y = 100.0
    Progress(progress)
    if !EndTree() { return -1 }
    return TreeCount()
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
    "$repo/tests/ziran_mixed_tree_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"
