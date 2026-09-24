#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root "$repo/tests" --module-path "$repo/src/ui" \
    "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --entry ziran_terminal_list_test:SelfTest -o "$work/source.zib" \
    "$repo/tests/ziran_terminal_list_test.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry ziran_terminal_list_test:SelfTest -o "$work/saved.zib" \
    "$work/ir/ziran_terminal_list_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
