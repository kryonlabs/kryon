#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_progress_raster_test.zi

set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_progress_raster_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_progress_raster_host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=ziran_progress_raster_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_progress_raster_host:RasterTextClipped \
    --bind raster:RasterLine=ziran_progress_raster_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_progress_raster_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry ziran_progress_raster_test:main \
    -o "$work/saved.zib" "$work/ir/ziran_progress_raster_test.zir"
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
mv "$work/go/ziran_progress_raster_test.go" "$work/go/progress_case.go"
cat > "$work/go/main.go" <<'GO'
package main

type progressHost struct{}

func (progressHost) RasterRoundedRectangle(bounds Rectangle, radius float32,
    segments int32, color Color) {
    ZiranProgressRasterHost_RasterRoundedRectangle(bounds, radius,
        segments, color)
}

func (progressHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    ZiranProgressRasterHost_RasterRoundedRectangleOutline(bounds, radius,
        segments, width, color)
}

func (progressHost) RasterText(value string, x, y, font int32, color Color) {
    ZiranProgressRasterHost_RasterText(value, x, y, font, color)
}

func (progressHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    ZiranProgressRasterHost_RasterTextClipped(value, x, y, font, color, clip)
}

func (progressHost) RasterLine(line Rectangle, color Color) {
    ZiranProgressRasterHost_RasterLine(line, color)
}

func (progressHost) RasterImage(path string, textureID uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) {
    ZiranProgressRasterHost_RasterImage(path, textureID, source, destination,
        clip, origin, rotation, radius, tint)
}

func main() {
    host := progressHost{}
    SetRasterShapeHost(host)
    SetRasterTextHost(host)
    SetRasterHost(host)
    SetPaintQueueHost(host)
    if ZiranProgressRasterTest_Main() != 0 { panic("progress paint failed") }
}
GO
GO111MODULE=off go run "$work/go"/*.go
