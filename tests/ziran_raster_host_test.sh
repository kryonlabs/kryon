#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_raster_host_test.zi
binding=raster:RasterLine=ziran_raster_host_provider:RasterLine

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --bind "$binding" --entry ziran_raster_host_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$binding" --entry ziran_raster_host_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_raster_host_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/cpp" "$source"
"${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$work/cpp" \
    "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root "$repo/tests" \
    --module-path "$repo/src/ui" -o "$work/go" "$source"
mv "$work/go/ziran_raster_host_test.go" "$work/go/raster_case.go"
cat > "$work/go/main.go" <<'GO'
package main

type testRasterHost struct{}

func (testRasterHost) RasterLine(line Rectangle, color Color) {
    ZiranRasterHostProvider_RasterLine(line, color)
}

func main() {
    SetRasterHost(testRasterHost{})
    if ZiranRasterHostTest_Main() != 0 { panic("raster behavior failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
