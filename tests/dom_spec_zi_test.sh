#!/bin/sh
set -eu

# Which element, role, and attributes each node gets is decided in Zi; the
# page only applies it. The five page effects are stubbed, so the rules run
# as native code without a browser.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$(CDPATH= cd -- "$bin/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_dir/std" \
    -o "$work/c" "$repo/tests/dom_spec_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "dom_spec_behavior.h"
void js_dom_begin(int32_t count, double width, double height)
{ (void)count; (void)width; (void)height; }
void js_dom_node(int32_t index, int32_t parent, double identity,
    int32_t widget_kind, int32_t semantic, int32_t level, double x, double y,
    double width, double height, uint8_t *tag, uint8_t *role, uint8_t *label,
    uint8_t *url, int32_t flags)
{
    (void)index; (void)parent; (void)identity; (void)widget_kind;
    (void)semantic; (void)level; (void)x; (void)y; (void)width; (void)height;
    (void)tag; (void)role; (void)label; (void)url; (void)flags;
}
void js_dom_title(uint8_t *title) { (void)title; }
void js_dom_finish(void) {}
void js_dom_close(void) {}
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
