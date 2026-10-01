#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
paths="--root $repo/tests --module-path $repo/src/ui --module-path $ziran_root/std"
source=$repo/tests/ziran_key_input_test.zi
"$ziran" bundle $paths --entry ziran_key_input_test:main -o "$work/keys.zib" "$source"
test "$("$ziran" run "$work/keys.zib")" = 0
"$ziran" build --target=c $paths --entry ziran_key_input_test:main -o "$work/c" "$source"
${CC:-cc} -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"
"$work/run-c"
echo 'Key input: typed text and text field editing keys passed'
