#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/typeface_source_behavior.zi

set -- \
    --bind kryon/typeface_source:LoadTypefaceData=typeface_source_host:LoadTypefaceData \
    --bind kryon/typeface_source:LoadTypefaceFile=typeface_source_host:LoadTypefaceFile \
    --bind kryon/typeface_source:LoadSystemTypeface=typeface_source_host:LoadSystemTypeface \
    --bind kryon/typeface_source:SelectTypeface=typeface_source_host:SelectTypeface \
    --bind kryon/typeface_source:ReleaseTypefaces=typeface_source_host:ReleaseTypefaces \
    --bind kryon/typeface_source:ReleaseTypefaceCpu=typeface_source_host:ReleaseTypefaceCpu
"$ziran" ir --root "$repo/tests" $kryon_project_args \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
    "$@" \
    --entry typeface_source_behavior:main -o "$work/source.zx" "$source"
"$ziran" bundle --project --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry typeface_source_behavior:main -o "$work/saved.zx" \
    "$work/ir/typeface_source_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir="$kryon_project_args"
    else
        module=$work/ir/typeface_source_behavior.zir
        module_root=$work/ir
        module_dir="--project --module-path $work/ir"
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry typeface_source_behavior:main "$@" \
        --root "$module_root" $module_dir \
        -o "$output" "$module"
    mv "$output/typeface_source_behavior.go" "$output/typeface_case.go"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
echo "Kryon typeface host contract test passed"
sh "$repo/tests/system_typeface_android_test.sh"
