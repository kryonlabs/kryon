set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0

work=$(mktemp -d "$repo/build/scratch/cui-motion-run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

source="$repo/tests/cui_motion_behavior.zi"
widget_test=cui_motion_behavior
widget_std="$ziran_root/std"
module_args="$kryon_project_args"

binds='
--bind image_raster:ImageWidth=cui_overlays_raster_host:CuiOverlaysImageWidth
--bind image_raster:ImageHeight=cui_overlays_raster_host:CuiOverlaysImageHeight
--bind font_metrics:MeasureGlyphWidth=cui_overlays_raster_host:CuiOverlaysMeasureGlyphWidth
--bind font_metrics:MeasureGlyphLineHeight=cui_overlays_raster_host:CuiOverlaysMeasureGlyphLineHeight
--bind raster_text:RasterText=cui_overlays_raster_host:CuiOverlaysRasterText
--bind raster_text:RasterTextClipped=cui_overlays_raster_host:CuiOverlaysRasterTextClipped
--bind raster_text:RasterTextRotated=cui_overlays_raster_host:CuiOverlaysRasterTextRotated
--bind raster_shape:RasterRoundedRectangle=cui_overlays_raster_host:CuiOverlaysRasterRoundedRectangle
--bind raster_shape:RasterRoundedRectangleOutline=cui_overlays_raster_host:CuiOverlaysRasterRoundedRectangleOutline
--bind raster:RasterLine=cui_overlays_raster_host:CuiOverlaysRasterLine
--bind paint_queue:RasterImage=cui_overlays_raster_host:CuiOverlaysRasterImage
--bind paint_queue:RasterImageMesh=cui_overlays_raster_host:CuiOverlaysRasterImageMesh'

# shellcheck disable=SC2086
"$ziran" ir --root "$repo/tests" $module_args \
    -o "$work/ir" "$source"
# shellcheck disable=SC2086
"$ziran" bundle --root "$repo/tests" $module_args $binds \
    --entry "$widget_test:main" -o "$work/source.zx" "$source"
# shellcheck disable=SC2086
"$ziran" bundle --root "$work/ir" --project --module-path "$work/ir" $binds \
    --entry "$widget_test:main" -o "$work/saved.zx" \
    "$work/ir/$widget_test.zir"

cmp "$work/source.zx" "$work/saved.zx"
result=$("$ziran" run "$work/source.zx")
echo "source motion result=$result"
test "$result" = 0
result=$("$ziran" run "$work/saved.zx")
echo "saved motion result=$result"
test "$result" = 0

for input in source saved; do
    if test "$input" = source; then
        root=$repo/tests
        module=$source
        paths=$module_args
    else
        root=$work/ir
        module=$work/ir/$widget_test.zir
        paths="--project --module-path $work/ir"
    fi

    output=$work/c-$input
    # shellcheck disable=SC2086
    "$ziran" build --target=c --entry "$widget_test:main" \
        --root "$root" $paths $binds -o "$output" "$module"
    # This driver only serializes the pixels painted by the Ziran native test.
    cat > "$output/evidence.c" <<'C'
#include "cui_motion_behavior.h"
#include <stdio.h>
#undef main
int main(int argc, char **argv) {
    int result = CuiMotionCheck(argc, argv);
    if (result) { fprintf(stderr, "native motion result=%d\n", result); return result; }
    const int frames[] = {0, 6, 90};
    if (argc != 2) return 1;
    for (int frame = 0; frame < 3; ++frame) {
        if (CuiMotionEvidence(frames[frame])) return 2;
        char path[2048];
        if (snprintf(path, sizeof path, "%s%d.ppm", argv[1], frames[frame]) >= (int)sizeof path) return 3;
        FILE *file = fopen(path, "wb");
        if (!file) return 4;
        fprintf(file, "P6\n480 360\n255\n");
        for (int i = 0; i < 480 * 360; ++i) {
            unsigned int pixel = CuiMotionPixel(i);
            unsigned char rgb[3] = {pixel >> 16, pixel >> 8, pixel};
            if (fwrite(rgb, 1, 3, file) != 3) return 5;
        }
        if (fclose(file)) return 6;
    }
    return 0;
}
C
    # shellcheck disable=SC2086
    "${CC:-cc}" -Dmain=CuiMotionCheck -O2 -std=c11 -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -I"$output" -I"$ziran_root/include" \
        "$output"/*.c "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
        -lm -lpthread -o "$output/app"
    "$output/app" "$work/$input-"

    output=$work/go-$input
    # shellcheck disable=SC2086
    "$ziran" build --target=go --pkg main --exe \
        --entry "$widget_test:main" --root "$root" $paths $binds \
        -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done

printf '%s\n' 'CUI motion local tests: source/saved ZX match; VM/C/Go behavior and native raster start/mid/end passed'

evidence="$repo/backup/cui-motion/evidence"
git -C "$repo" check-ignore -q "$evidence/start.webp"
mkdir -p "$evidence"
python3 - "$work" "$evidence" <<'PYCODE'
from PIL import Image
from pathlib import Path
import sys
work, evidence = map(Path, sys.argv[1:])
for frame, label in [(0, 'start'), (6, 'middle'), (90, 'end')]:
    source = work / f'source-{frame}.ppm'
    assert source.read_bytes() == (work / f'saved-{frame}.ppm').read_bytes(), frame
    with Image.open(source) as image:
        image.save(evidence / f'{label}.webp', lossless=True)
        for name, box in [('menu',(10,60,210,250)), ('tabs',(10,10,410,54)),
                          ('collapsible',(220,80,460,260)), ('navigation',(0,280,480,360))]:
            image.crop(box).save(evidence / f'{name}-{label}.webp', lossless=True)
print('Native source/saved pixels match; WebP evidence:', evidence)
PYCODE
