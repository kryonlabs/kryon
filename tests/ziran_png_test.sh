#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/png-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
python3 "$repo/tests/png_fixtures.py" --zi "$work/png_cases.zi"
set -- --root "$work" --module-path "$repo/src/backend" --module-path "$ziran_root/std"
"$ziran" ir "$@" -o "$work/ir" "$work/png_cases.zi"
"$ziran" bundle "$@" --entry png_cases:main -o "$work/source.zib" "$work/png_cases.zi"
"$ziran" bundle --root "$work/ir" --entry png_cases:main -o "$work/saved.zib" "$work/ir/png_cases.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0
for target in c cpp; do
    "$ziran" build --target="$target" "$@" -o "$work/$target-source" "$work/png_cases.zi"
    "$ziran" build --target="$target" --root "$work/ir" -o "$work/$target-saved" "$work/ir/png_cases.zir"
    for form in source saved; do
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/$target-$form" \
                "$work/$target-$form"/*.c -o "$work/$target-$form/run"
        else
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$work/$target-$form" \
                "$work/$target-$form"/*.cpp -o "$work/$target-$form/run"
        fi
        "$work/$target-$form/run"
    done
done
for form in source saved; do
    if test "$form" = source; then
        "$ziran" build --target=go --pkg main "$@" -o "$work/go-$form" "$work/png_cases.zi"
    else
        "$ziran" build --target=go --pkg main --root "$work/ir" -o "$work/go-$form" "$work/ir/png_cases.zir"
    fi
    mv "$work/go-$form/png_cases.go" "$work/go-$form/cases.go"
    cat > "$work/go-$form/main.go" <<'GO'
package main
func main() {
    if PngCases_Main() != 0 { panic("incorrect PNG pixels") }
}
GO
    GO111MODULE=off go run "$work/go-$form"/*.go
done
echo 'PNG: source/saved IR pixels match in portable bundles, C, C++, and Go'
