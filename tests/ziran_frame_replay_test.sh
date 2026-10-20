#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work="$repo/build/scratch/frame-replay"
rm -rf "$work"
mkdir -p "$work/c"
"$ziran" build --target=c --no-main --entry frame_replay_test:Check --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/frame_replay_test.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "frame_replay_test.h"
int main(void) {
    int result = Check();
    if (result) fprintf(stderr, "frame replay check %d failed\n", result);
    return result;
}
C
includes=$(find "$work/c" -type d -printf ' -iquote %p')
# shellcheck disable=SC2046,SC2086
"${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" $includes $(find "$work/c" -name '*.c') \
    "$ziran_root/build/libziran.a" -lm -lpthread -o "$work/test"
"$work/test"
echo 'Frame replay: unchanged reads replay copied widget calls; changed reads or widgets run the app again'
