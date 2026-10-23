#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
paths="--root $repo/tests --module-path $repo/src/ui --module-path $ziran_root/std"
source=$repo/tests/ziran_key_input_test.zi
"$ziran" bundle $paths --entry ziran_key_input_test:main -o "$work/keys.zx" "$source"
test "$("$ziran" run "$work/keys.zx")" = 0
"$ziran" build --target=c $paths --entry ziran_key_input_test:main -o "$work/c" "$source"
${CC:-cc} -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"
"$work/run-c"
echo 'Key input: typed text and text field editing keys passed'
