#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/ziran_image_canvas_test.zi
set -- --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --module-path "$ziran_root/std"

"$ziran" ir --root "$repo/tests" "$@" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" "$@" \
    --entry ziran_image_canvas_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry ziran_image_canvas_test:main -o "$work/saved.zx" \
    "$work/ir/ziran_image_canvas_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    "$@" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    "$@" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -lm -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    "$@" -o "$work/go" "$source"
cat > "$work/go/main.go" <<'GO'
package main
func main() {
    if ZiranImageCanvasTest_Main() != 0 { panic("wrong image pixels") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
