#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_composition_input_test.zi
binding=composition_input:PollComposition=ziran_composition_input_host:PollComposition

"$ziran" ir --root "$repo/tests" $kryon_project_args \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
    --bind "$binding" --entry ziran_composition_input_test:main \
    -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --project --module-path "$work/ir" \
    --bind "$binding" --entry ziran_composition_input_test:main \
    -o "$work/saved.zx" "$work/ir/ziran_composition_input_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    $kryon_project_args -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"


"$ziran" build --target=go --pkg main --root "$repo/tests" \
    $kryon_project_args -o "$work/go" "$source"
cat > "$work/go/main.go" <<'GO'
package main

type compositionHost struct{}

func (compositionHost) PollComposition() CompositionSample {
    return ZiranCompositionInputHost_PollComposition()
}

func main() {
    SetCompositionInputHost(compositionHost{})
    if ZiranCompositionInputTest_Main() != 0 { panic("composition failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
