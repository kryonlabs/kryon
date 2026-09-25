#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_composition_queue_test.zi

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    -o "$work/ir" "$source"

for input in source saved; do
    if test "$input" = source; then
        module=$source
        root=$repo/tests
        path=$repo/src/backend
    else
        module=$work/ir/ziran_composition_queue_test.zir
        root=$work/ir
        path=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                --module-path "$path" -o "$output" "$module"
        else
            "$ziran" build --target="$target" --root "$root" \
                --module-path "$path" -o "$output" "$module"
        fi
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            mv "$output/ziran_composition_queue_test.go" "$output/queue_case.go"
            cat > "$output/main.go" <<'GO'
package main

func main() {
    if ZiranCompositionQueueTest_Main() != 0 { panic("composition queue failed") }
}
GO
            GO111MODULE=off go run "$output"/*.go
        fi
    done
done
