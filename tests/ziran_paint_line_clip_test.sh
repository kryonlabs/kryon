#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/paint-line-clip.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
module=ziran_paint_line_clip_test
entry=$module:Main
binding=kryon/raster:RasterLine=$module:RecordLine
source=$repo/tests/$module.zi

"$ziran" ir --project --bind "$binding" --entry "$entry" --root "$repo/tests" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    set -- --bind "$binding"
    if [ "$form" = saved ]; then
        root=$work/ir
        input=$root/$module.zir
        set --
    fi
    "$ziran" build --project "$@" --target=c --exe --root "$root" --entry "$entry" -o "$work/$form" "$input"
    "$work/$form/$module"
    "$ziran" bundle --project "$@" --root "$root" --entry "$entry" -o "$work/$form.zx" "$input"
    result=$("$ziran" run --project "$work/$form.zx")
    if [ "$result" != 0 ]; then
        printf '%s\n' "$result" >&2
        exit 1
    fi
done
echo 'Paint lines clip at both endpoints in native C and ZX from source and saved IR'
