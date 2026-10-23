#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry ziran_terminal_list_test:SelfTest -o "$work/source.zx" \
    "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry ziran_terminal_list_test:SelfTest -o "$work/saved.zx" \
    "$work/ir/ziran_terminal_list_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
