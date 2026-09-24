#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "cursor"

stage :: i32 #global
state :: CursorFrame #global

Frame :: () -> i32 #export {
    state = CursorBeginFrame(state)
    if stage == 0 {
        state = CursorRequest(state, (i32)CursorClickable)
        state = CursorRequest(state, (i32)CursorDisabled)
    } else if stage == 1 {
        state = CursorRequest(state, (i32)CursorText)
        state = CursorRequest(state, (i32)CursorResizeHorizontal)
    } else if stage == 4 {
        state = CursorRequest(state, (i32)CursorText)
        state = CursorRequest(state, (i32)CursorClickable)
    } else if stage == 5 {
        state = CursorRequest(state, (i32)CursorClickable)
        state = CursorRequest(state, (i32)CursorCrosshair)
    } else if stage == 6 {
        state = CursorRequest(state, 99)
    }
    decision: CursorDecision = CursorEndFrame(state)
    if stage == 0 && (!decision.apply ||
        decision.state.applied != (i32)CursorClickable) { return -1 }
    if stage == 1 && (!decision.apply ||
        decision.state.applied != (i32)CursorResizeHorizontal) { return -2 }
    if stage == 2 && (!decision.apply ||
        decision.state.applied != (i32)CursorDefault) { return -3 }
    if stage == 3 && decision.apply { return -4 }
    if stage == 4 && (!decision.apply ||
        decision.state.applied != (i32)CursorClickable) { return -5 }
    if stage == 5 && (!decision.apply ||
        decision.state.applied != (i32)CursorCrosshair) { return -6 }
    if stage == 6 && (!decision.apply ||
        decision.state.applied != (i32)CursorDefault) { return -7 }
    state = CommitCursor(decision)
    old: i32 = stage
    stage += 1
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

cat > "$work/host.c" <<'C'
#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

typedef struct Seen {
    int shapes[8];
    int count;
} Seen;

static void set_shape(void *context, int shape)
{
    Seen *seen = context;
    assert(seen->count < 8);
    seen->shapes[seen->count++] = shape;
}

int main(int argc, char **argv)
{
    const int expected[] = {4, 5, 0, 4, 3, 0};
    Seen seen = {{0}, 0};
    CursorPlatform platform = {set_shape, &seen};
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 1);
    assert(strcmp(BundleCapabilityModule(bundle, 0), "cursor") == 0);
    assert(strcmp(BundleCapabilityFunction(bundle, 0),
                  "ApplyCursorShape") == 0);
    HostBinding binding = CursorBinding(&platform);
    BundleInstance *instance = BundleInstantiate(bundle, &binding, 1);
    assert(instance != NULL);
    for(int stage = 0; stage < 7; stage++) {
        long long result = -1;
        int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == stage);
    }
    assert(seen.count == 6);
    for(int i = 0; i < seen.count; i++)
        assert(seen.shapes[i] == expected[i]);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
C

"${CC:-cc}" -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" -I"$repo/../ziran/include" \
    -o "$work/host" "$work/host.c" \
    "$repo/build/ziran/libkryon_host.a" "$repo/../ziran/build/libziran.a"
"$work/host" "$work/source.zib"
"$work/host" "$work/saved.zib"
