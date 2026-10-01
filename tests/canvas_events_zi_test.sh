#!/bin/sh
set -eu

# The browser input policy runs as ordinary native code here: the DOM is
# replaced by a fake page module, so every decision the page's
# JavaScript used to make is checked without a browser.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$(CDPATH= cd -- "$bin/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --define PLATFORM_WEB --root "$repo/tests" \
    --module-path "$repo/tests/page_fake" --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    --module-path "$ziran_dir/std" \
    -o "$work/c" "$repo/tests/canvas_events_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "canvas_events_behavior.h"
int main(void)
{
    int result = Answer();
    if (result != 42) fprintf(stderr, "canvas events check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function \
    -I"$ziran_dir/include" -I"$work/c" "$work"/c/*.c -o "$work/test"
"$work/test"
echo "Kryon browser input policy passed"
