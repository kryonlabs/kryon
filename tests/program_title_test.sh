#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/program-title.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/program_title_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/program_title_test.zir; fi
    output=$work/$form
    "$ziran" build --target=c --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$output" "$input"
    ${CC:-cc} -std=c11 -O1 -I"$output" "$output"/*.c -lm -o "$output/probe"
    mkdir -p "$output/beside" "$output/prefix/bin" "$output/prefix/share/kryon/probe" "$output/bare"
    cp "$output/probe" "$output/beside/probe"
    printf '%s\n' 'Beside Name' > "$output/beside/probe.title"
    cp "$output/probe" "$output/prefix/bin/probe"
    printf '%s\n' 'Installed Name' > "$output/prefix/share/kryon/probe/app.title"
    cp "$output/probe" "$output/bare/probe"
    "$output/beside/probe" 'Beside Name'
    "$output/prefix/bin/probe" 'Installed Name'
    "$output/bare/probe" probe
done
echo 'Program titles: beside, installed and file-name lookups passed C source and saved IR'
