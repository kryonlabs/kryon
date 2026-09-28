#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_wrap_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_text_wrap_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_text_wrap_host:MeasureGlyphLineHeight

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" "$@" \
    --entry ziran_text_wrap_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_text_wrap_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_text_wrap_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_text_wrap_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_text_wrap_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$repo/../ziran/std" -o "$output" "$module"
    mv "$output/ziran_text_wrap_test.go" "$output/text_wrap_case.go"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
