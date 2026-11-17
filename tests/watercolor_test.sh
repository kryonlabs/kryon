#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/watercolor"
mkdir -p "$out"
# The module is standalone; this also tests its source and saved IR before
# the package export is wired by the owning integration lane.
args="--module-path $repo/src/ui --module-path std=$ziran_root/std"
"$ziran_root/build/bin/zi2zir" --root "$repo/tests" $args -o "$out/ir" "$repo/tests/watercolor_test.zi"
for form in source saved; do
    source="$repo/tests/watercolor_test.zi"
    root="$repo/tests"
    if test "$form" = saved; then
        source="$out/ir/watercolor_test.zir"
        root="$out/ir"
    fi
    "$ziran_root/build/bin/zi2c" --root "$root" $args --entry watercolor_test:main --exe -o "$out/$form" "$source"
    "$out/$form/watercolor_test"
    "$ziran_root/build/bin/zi2zx" --root "$root" $args --entry watercolor_test:main -o "$out/$form.zx" "$source"
    test "$("$ziran" run "$out/$form.zx")" = 0
done
printf 'Watercolor source/saved native and portable local tests passed\n'
