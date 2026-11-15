#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/daylight"
mkdir -p "$out"
source="$repo/tests/daylight_test.zi"
"$ziran" build --project --root "$repo/tests" --target=c --entry daylight_test:main --exe -o "$out/native" "$source"
"$out/native/daylight_test"
"$ziran" bundle --project --root "$repo/tests" --entry daylight_test:main -o "$out/daylight.zx" "$source"
test "$("$ziran" run "$out/daylight.zx" | tail -n 1)" = 0
printf 'Daylight native and portable passed\n'
