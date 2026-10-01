#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_transition_fade_widget_test.zi
binding=raster_shape:RasterRoundedRectangle=ziran_transition_fade_host:RasterRoundedRectangle

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --bind "$binding" --entry ziran_transition_fade_widget_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$binding" --entry ziran_transition_fade_widget_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_transition_fade_widget_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/go" "$source"
cat > "$work/go/main.go" <<'GO'
package main

type fadeHost struct{}

func (fadeHost) RasterRoundedRectangle(bounds Rectangle, radius float32,
    segments int32, color Color) {
    ZiranTransitionFadeHost_RasterRoundedRectangle(bounds, radius, segments, color)
}

func (fadeHost) RasterRoundedRectangleOutline(bounds Rectangle, radius float32,
    segments int32, width float32, color Color) {
    ZiranTransitionFadeHost_RasterRoundedRectangleOutline(bounds, radius,
        segments, width, color)
}

func main() {
    SetRasterShapeHost(fadeHost{})
    if ZiranTransitionFadeWidgetTest_Main() != 0 { panic("fade behavior failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
