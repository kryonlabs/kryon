# Shared source/saved-IR bundle and Go matrix. The caller supplies its own
# provider bindings, standard library, source file and scratch cleanup.
widget_test=${source##*/}
widget_test=${widget_test%.zi}

"$ziran" ir --root "$repo/tests" $kryon_project_args \
     -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
     "$@" \
    --entry "$widget_test:main" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --project --module-path "$work/ir" "$@" \
    --entry "$widget_test:main" -o "$work/saved.zx" \
    "$work/ir/$widget_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran_root/build/bin/zi2zx" run --vm "$work/source.zx")" = 0

if test "${widget_run_saved:-0}" = 1; then
    test "$("$ziran_root/build/bin/zi2zx" run --vm "$work/saved.zx")" = 0
fi

if test "${widget_native:-0}" = 1; then
    for target in c; do
        output=$work/native-$target
        "$ziran" build --target="$target" --root "$repo/tests" \
            $kryon_project_args \
            -o "$output" "$source"
            "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$output" \
                "$output"/*.c -o "$output/app"
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
    done
fi

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir="$kryon_project_args"
    else
        module=$work/ir/$widget_test.zir
        module_root=$work/ir
        module_dir="--project --module-path $work/ir"
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry "$widget_test:main" "$@" \
        --root "$module_root" $module_dir \
         -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
