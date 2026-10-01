#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/typeface_source_behavior.zi

set -- \
    --bind typeface_source:LoadTypefaceData=typeface_source_host:LoadTypefaceData \
    --bind typeface_source:LoadTypefaceFile=typeface_source_host:LoadTypefaceFile \
    --bind typeface_source:LoadSystemTypeface=typeface_source_host:LoadSystemTypeface \
    --bind typeface_source:SelectTypeface=typeface_source_host:SelectTypeface \
    --bind typeface_source:ReleaseTypefaces=typeface_source_host:ReleaseTypefaces \
    --bind typeface_source:ReleaseTypefaceCpu=typeface_source_host:ReleaseTypefaceCpu
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry typeface_source_behavior:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry typeface_source_behavior:main -o "$work/saved.zib" \
    "$work/ir/typeface_source_behavior.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/typeface_source_behavior.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry typeface_source_behavior:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$ziran_root/std" -o "$output" "$module"
    mv "$output/typeface_source_behavior.go" "$output/typeface_case.go"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
echo "Kryon typeface host contract test passed"
