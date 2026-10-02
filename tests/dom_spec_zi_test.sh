#!/bin/sh
set -eu

# Which element, role, and attributes each node gets is decided in Zi; the
# page only applies it. The page is replaced by a fake module, so the rules run
# as native code without a browser.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$ziran_root
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --root "$repo/tests" --module-path "$repo/tests/dom_fake" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_dir/std" \
    -o "$work/c" "$repo/tests/dom_spec_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "dom_spec_behavior.h"
int main(void)
{
    int result = Answer();
    if (result != 42) fprintf(stderr, "dom spec check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function \
    -I"$ziran_dir/include" -I"$work/c" "$work"/c/*.c -lm -o "$work/test"
"$work/test"
echo "Kryon DOM element policy passed"
