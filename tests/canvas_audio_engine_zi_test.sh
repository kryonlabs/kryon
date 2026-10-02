#!/bin/sh
set -eu

# The browser audio state machines run as native code against a stand-in
# for the audio host (tests/canvas_audio_fake) with a clock the test
# controls, so positions, scheduling, and limits are checked exactly without
# a browser or a sound card. The fake is found ahead of the real host on the
# module path.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$ziran_root
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --define PLATFORM_WEB --root "$repo/tests" \
    --module-path "$repo/tests/canvas_audio_fake" \
    --module-path "$repo/src/backend" --module-path "$ziran_dir/std" \
    -o "$work/c" "$repo/tests/canvas_audio_engine_behavior.zi"
if grep -q 'js_web_' "$work/c"/*.c; then
    echo "the real audio host was used instead of the fake" >&2
    exit 1
fi
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "canvas_audio_engine_behavior.h"
int main(void)
{
    int result = Answer();
    if (result != 42) fprintf(stderr, "audio engine check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function \
    -I"$ziran_dir/include" -I"$work/c" "$work"/c/*.c -lm -o "$work/test"
"$work/test"
echo "Kryon browser audio state passed"
