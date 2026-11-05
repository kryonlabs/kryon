#!/bin/sh
set -eu

# The web canvas page's sizing and pointer mapping (page_geometry.zi): a
# canvas in a frame padded by safe-area insets draws at its own size, and a
# touch maps onto what is drawn under it.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/page-geometry-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

"$bin/zi2c" --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" "$repo/tests/page_geometry_test.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "page_geometry_test.h"
int main(void)
{
    int result = GeometryCheck();
    if (result != 42) fprintf(stderr, "page geometry check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
includes=$(find "$work/c" -type d -printf ' -iquote %p')
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" $includes "$work"/c/*.c -lm -o "$work/test"
"$work/test"
echo "page geometry: padded canvas sizing and pointer mapping passed"
