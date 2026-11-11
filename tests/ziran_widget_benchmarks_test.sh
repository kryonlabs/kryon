#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/widget_benchmarks.zi
set --
for binding in $(python3 -c 'import json,sys; print("\n".join(json.load(open(sys.argv[1]))["bindings"]))' "$repo/tests/widget_benchmarks.json"); do
    set -- "$@" --bind "$binding"
done
"$ziran" ir --root "$repo/tests" --project --module-path "$repo/tests" -o "$work/ir" "$source"
for form in source saved; do
    root=$repo/tests
    input=$source
    if test "$form" = saved; then root=$work/ir; input=$root/widget_benchmarks.zir; fi
    "$ziran" bundle --root "$root" --project --module-path "$root" "$@" \
        --entry widget_benchmarks:main -o "$work/$form.zx" "$input"
    test "$("$ziran" run --vm "$work/$form.zx")" = 0
    "$ziran" build --target=c --exe --entry widget_benchmarks:main \
        --root "$root" --project --module-path "$root" "$@" -o "$work/c-$form" "$input"
    "$work/c-$form/widget_benchmarks"
    "$ziran" build --target=go --pkg main --exe --entry widget_benchmarks:main \
        --root "$root" --project --module-path "$root" "$@" -o "$work/go-$form" "$input"
    GO111MODULE=off go run "$work/go-$form"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Repeatable public Button, Text and TextField scenarios preserve layout, activation, UTF-8 editing and consumed paint payloads'
