#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/input-scope.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export GOCACHE="$work/go-cache"
export GOMAXPROCS=1
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$repo/tests/input_scope_test.zi"
for form in source saved; do
    root=$repo/tests
    modules=$repo/src/ui
    input=$repo/tests/input_scope_test.zi
    if test "$form" = saved; then
        root=$work/ir
        modules=$work/ir
        input=$work/ir/input_scope_test.zir
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" \
        --module-path "$ziran_root/std" --entry input_scope_test:Answer \
        -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
    for target in c cpp go; do
        out=$work/$form-$target
        target_flags=
        if test "$target" = go; then target_flags='--pkg main'; fi
        "$ziran" build --root "$root" --module-path "$modules" \
            --module-path "$ziran_root/std" --target="$target" $target_flags \
            --entry input_scope_test:main -o "$out" "$input"
        case "$target" in
            c) "${CC:-cc}" -std=c11 -O1 -I"$out" "$out"/*.c -o "$out/run" ;;
            cpp) "${CXX:-c++}" -std=c++17 -O1 -I"$out" "$out"/*.cpp -o "$out/run" ;;
            go)
                cat > "$out/main.go" <<'GO'
package main
func main() {
    if InputScopeTest_Answer() != 42 { panic("input scope failed") }
}
GO
                (cd "$out" && GO111MODULE=off go build -o run .) ;;
        esac
        timeout --kill-after=2s 15s "$out/run"
    done
done
cmp "$work/source.zib" "$work/saved.zib"
