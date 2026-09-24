#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "composition_input"
#import "text_input"

phase :: i32 #global
state :: CompositionState #global

Frame :: () -> i32 #export {
    sample: CompositionSample = SampleComposition()
    transition: CompositionTransition = CompositionTransitionFor(
        state, CompositionEventForSample(sample), true, false)
    state = transition.state
    if phase == 0 {
        if !sample.available ||
            sample.phase != (CompositionPhase)CompositionStart ||
            !state.active || state.text != "é" ||
            state.cursor != 0 || state.selection_length != 2 {
            return -1
        }
    } else if phase == 1 {
        if sample.available || !state.active ||
            state.text != "é" || transition.changed {
            return -2
        }
    } else if phase == 2 {
        if !sample.available || !state.active ||
            state.text != "éx" || state.cursor != 3 {
            return -3
        }
    } else if phase == 3 {
        if !sample.available || !transition.commit ||
            transition.commit_text != "Q" ||
            state.active { return -4 }
    } else if phase == 4 {
        if !sample.available || !state.active ||
            state.text != "A" { return -5 }
        canceled: CompositionSample = SampleComposition()
        if !canceled.available ||
            canceled.phase != (CompositionPhase)CompositionCancel {
            return -6
        }
        after: CompositionTransition = CompositionTransitionFor(
            state, CompositionEventForSample(canceled), true, false)
        state = after.state
        if state.active || !after.changed { return -7 }
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

cat > "$work/host.c" <<'C'
#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static void submit(CompositionQueue *queue, int phase)
{
    if(phase == 0)
        assert(CompositionQueueSubmit(queue, 1, "é", 1, 99));
    if(phase == 2)
        assert(CompositionQueueSubmit(queue, 2, "éx", 3, 0));
    if(phase == 3)
        assert(CompositionQueueSubmit(queue, 3, "Q", 0, 0));
    if(phase == 4) {
        assert(CompositionQueueSubmit(queue, 1, "A", 1, 0));
        assert(CompositionQueueSubmit(queue, 4, "", 0, 0));
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    CompositionQueue *queue = CompositionQueueCreate();
    assert(queue != NULL);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    HostBinding binding = PollCompositionBinding(queue);
    BundleInstance *instance = BundleInstantiate(bundle, &binding, 1);
    assert(instance != NULL);
    for(int phase = 0; phase < 5; phase++) {
        long long value = -1;
        int has_value = 0;
        CompositionQueueBeginFrame(queue);
        submit(queue, phase);
        assert(BundleInstanceRun(instance, &value, &has_value));
        assert(has_value && value == phase);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);

    char long_text[258];
    memset(long_text, 'a', 254);
    long_text[254] = (char)0xc3;
    long_text[255] = (char)0xa9;
    long_text[256] = '\0';
    CompositionQueueBeginFrame(queue);
    assert(CompositionQueueSubmit(queue, 2, long_text, -1, -1));
    CompositionInputEvent event;
    assert(CompositionQueueTake(queue, &event));
    assert(event.available && event.length == 254 &&
           event.cursor == 0 && event.selection_length == 0);
    assert(!CompositionQueueTake(queue, &event) && !event.available);
    CompositionQueueBeginFrame(queue);
    assert(!CompositionQueueSubmit(queue, 0, "", 0, 0));
    for(int i = 0; i < 16; i++)
        assert(CompositionQueueSubmit(queue, 1, "x", 0, 0));
    assert(!CompositionQueueSubmit(queue, 1, "x", 0, 0));
    CompositionQueueDestroy(queue);
    return 0;
}
C

"${CC:-cc}" -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" "$work/host.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#define HOST
#endif
#include "kryon_portable_host.h"

static CompositionQueue *queue;

HOST CompositionSample PollComposition(void)
{
    CompositionInputEvent raw;
    CompositionSample sample = {0};
    (void)CompositionQueueTake(queue, &raw);
    sample.available = raw.available != 0;
    sample.phase = (CompositionPhase)raw.phase;
    sample.text = (String){raw.text, raw.length};
    sample.cursor = raw.cursor;
    sample.selection_length = raw.selection_length;
    return sample;
}

int main(void)
{
    queue = CompositionQueueCreate();
    assert(queue != NULL);
    for(int phase = 0; phase < 5; phase++) {
        CompositionQueueBeginFrame(queue);
        if(phase == 0)
            assert(CompositionQueueSubmit(queue, 1, "é", 1, 99));
        if(phase == 2)
            assert(CompositionQueueSubmit(queue, 2, "éx", 3, 0));
        if(phase == 3)
            assert(CompositionQueueSubmit(queue, 3, "Q", 0, 0));
        if(phase == 4) {
            assert(CompositionQueueSubmit(queue, 1, "A", 1, 0));
            assert(CompositionQueueSubmit(queue, 4, "", 0, 0));
        }
        assert(Frame() == phase);
    }
    CompositionQueueDestroy(queue);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
            -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c "$repo/build/ziran/libkryon_host.a" \
            -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/build/ziran/c" -I"$repo/include" \
            -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp "$repo/build/ziran/libkryon_host.a" \
            -o "$output/app"
        "$output/app"
    else
        cat > "$output/composition_input_test.go" <<'GO'
package ziran
import "testing"
type sampleHost struct {
    events []CompositionSample
    position int
}
func (h *sampleHost) PollComposition() CompositionSample {
    if h.position >= len(h.events) { return CompositionSample{} }
    sample := h.events[h.position]
    h.position++
    return sample
}
func TestCompositionInput(t *testing.T) {
    h := &sampleHost{events: []CompositionSample{
        {Available:true, Phase:CompositionPhaseCompositionStart,
            Text:"é", Cursor:1, SelectionLength:99},
        {},
        {Available:true, Phase:CompositionPhaseCompositionUpdate,
            Text:"éx", Cursor:3},
        {Available:true, Phase:CompositionPhaseCompositionCommit, Text:"Q"},
        {Available:true, Phase:CompositionPhaseCompositionStart, Text:"A", Cursor:1},
        {Available:true, Phase:CompositionPhaseCompositionCancel},
    }}
    SetCompositionInputHost(h)
    for phase := int32(0); phase < 5; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
