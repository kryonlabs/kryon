#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    "$repo/tests/ziran_cell_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$repo/tests/ziran_cell_test.zi"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry ziran_cell_test:SelfTest -o "$work/source.zib" \
    "$repo/tests/ziran_cell_test.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry ziran_cell_test:SelfTest -o "$work/saved.zib" \
    "$work/ir/ziran_cell_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
