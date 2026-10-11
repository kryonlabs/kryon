#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/browser-viewport-test
mkdir -p "$work"
"$ziran_root/build/bin/zi2c" --no-main --root "$repo/tests" --module-path "$ziran_root/std" \
    --module-path "$repo/src/backend" -o "$work/c" "$repo/tests/browser_viewport_behavior.zi"
includes="-I$ziran_root/include -I$work/c"
for header in $(find "$work/c" -name '*.h' | xargs -n1 dirname | sort -u); do
    includes="$includes -I$header"
done
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 $includes $(find "$work/c" -name '*.c') -o "$work/test"
"$work/test"
printf '%s\n' 'Browser viewport fallback and resize settling passed'
