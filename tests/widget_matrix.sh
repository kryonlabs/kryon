# Shared source/saved-IR bundle and Go matrix. The caller supplies its own
# provider bindings, standard library, source file and scratch cleanup.
widget_test=${source##*/}
widget_test=${widget_test%.zi}

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$widget_std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$widget_std" "$@" \
    --entry "$widget_test:main" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry "$widget_test:main" -o "$work/saved.zx" \
    "$work/ir/$widget_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/$widget_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry "$widget_test:main" "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$widget_std" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done
