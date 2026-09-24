#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "text_input"

phase: s32;
used: s32;
buffer: [12]u8;

#program_export
Frame :: () -> s32 {
    if phase == 0 {
        buffer[0] = cast(u8)65
        buffer[1] = cast(u8)66
        buffer[2] = cast(u8)0
        used = 2
        accent: [2]u8
        accent[0] = cast(u8)0xc3
        accent[1] = cast(u8)0xa9
        edit: TextBufferEditResult = TextBufferInsertBytes(
            buffer[0:12], used, 1, accent[0:2])
        if !edit.changed || edit.length != 4 ||
            edit.cursor != 3 || buffer[0] != cast(u8)65 ||
            buffer[1] != cast(u8)0xc3 ||
            buffer[2] != cast(u8)0xa9 ||
            buffer[3] != cast(u8)66 || buffer[4] != cast(u8)0 ||
            TextPreviousCodepoint(buffer[0:12], 4, 3) != 1 ||
            TextNextCodepoint(buffer[0:12], 4, 1) != 3 {
            return -1
        }
        used = edit.length
    } else if phase == 1 {
        edit: TextBufferEditResult = TextBufferDeleteRange(
            buffer[0:12], used, 1,
            TextNextCodepoint(buffer[0:12], used, 1))
        if !edit.changed || edit.length != 2 ||
            edit.cursor != 1 || buffer[0] != cast(u8)65 ||
            buffer[1] != cast(u8)66 || buffer[2] != cast(u8)0 {
            return -2
        }
        used = edit.length
    } else if phase == 2 {
        tail: [3]u8
        tail[0] = cast(u8)88
        tail[1] = cast(u8)89
        tail[2] = cast(u8)90
        edit: TextBufferEditResult = TextBufferInsertBytes(
            buffer[0:12], used, 2, tail[0:3])
        if !edit.changed || edit.length != 5 ||
            buffer[2] != cast(u8)88 || buffer[3] != cast(u8)89 ||
            buffer[4] != cast(u8)90 || buffer[5] != cast(u8)0 {
            return -3
        }
        used = edit.length
    } else if phase == 3 {
        edit: TextBufferEditResult = TextBufferDeleteRange(
            buffer[0:12], used, 1, 4)
        if !edit.changed || edit.length != 2 ||
            edit.cursor != 1 || buffer[0] != cast(u8)65 ||
            buffer[1] != cast(u8)90 || buffer[2] != cast(u8)0 {
            return -4
        }
        used = edit.length
    } else if phase == 4 {
        too_large: [10]u8
        edit: TextBufferEditResult = TextBufferInsertBytes(
            buffer[0:12], used, 1, too_large[0:10])
        if edit.changed || edit.length != 2 ||
            buffer[0] != cast(u8)65 || buffer[1] != cast(u8)90 ||
            buffer[2] != cast(u8)0 ||
            TextPreviousCodepoint(buffer[0:12], used, 0) != 0 ||
            TextNextCodepoint(buffer[0:12], used, used) != used {
            return -5
        }
    } else if phase == 5 {
        invalid: [2]u8
        invalid[0] = cast(u8)0
        invalid[1] = cast(u8)66
        edit: TextBufferEditResult = TextBufferInsertBytes(
            buffer[0:12], used, 1, invalid[0:2])
        if edit.changed || edit.length != 2 ||
            buffer[0] != cast(u8)65 || buffer[1] != cast(u8)90 ||
            buffer[2] != cast(u8)0 { return -6 }
    }
    old: s32 = phase
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/../ziran/include" \
    "$repo/tests/ziran_text_buffer_test.c" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cat > "$output/main.c" <<'C'
#include "app.h"
#include <assert.h>
int main(void) {
    for (int phase = 0; phase < 6; phase++)
        assert(Frame() == phase);
    return 0;
}
C
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
#include <cassert>
int main() {
    for (int phase = 0; phase < 6; phase++)
        assert(Frame() == phase);
    return 0;
}
CPP
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/text_buffer_test.go" <<'GO'
package ziran
import "testing"
func TestTextBuffer(t *testing.T) {
    for phase := int32(0); phase < 6; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done
