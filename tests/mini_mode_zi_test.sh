#!/bin/sh
set -eu

# Mini-mode requests and the full app's address, shared by every app that
# offers an embeddable mini view.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$ziran_root
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_dir/std" -o "$work/c" "$repo/tests/mini_mode_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "mini_mode_behavior.h"
int main(void)
{
    int result = Answer();
    if (result != 42) fprintf(stderr, "mini mode check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function -DZIRAN_BOUNDS_CHECK \
    -I"$ziran_dir/include" -I"$work/c" "$work"/c/*.c -o "$work/test"
"$work/test"
echo "Kryon mini mode passed"
