#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/ziran_image_canvas_test.zi
set -- --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std"

"$ziran" ir --root "$repo/tests" "$@" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" "$@" \
    --entry ziran_image_canvas_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry ziran_image_canvas_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_image_canvas_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    "$@" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    "$@" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -lm -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    "$@" -o "$work/go" "$source"
mv "$work/go/ziran_image_canvas_test.go" "$work/go/canvas_case.go"
cat > "$work/go/main.go" <<'GO'
package main
func main() {
    if ZiranImageCanvasTest_Main() != 0 { panic("wrong image pixels") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
