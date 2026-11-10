#!/bin/sh
# Smooth images through the Cairo host: once its Lanczos copy is made, a
# picture is drawn from it pixel for pixel, the same as Pillow's
# Image.resize(LANCZOS, box) composited over the frame.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
. "$repo/tests/image_packages.sh"
work="$repo/build/scratch/cairo-scaled-image"
rm -rf "$work"
mkdir -p "$work/fixtures"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
python3 "$repo/tests/lanczos_goldens.py" fixtures "$work/fixtures"
python3 - "$work/fixtures" <<'PY'
from pathlib import Path
from PIL import Image
import sys
for kind, alpha in enumerate((255, 128)):
    Image.new('RGBA', (512, 512), (20, 70, 110, alpha)).save(Path(sys.argv[1]) / f'join-{kind}.png')
PY
libs="$(pkg-config --libs cairo freetype2 pangocairo fontconfig glib-2.0) -ldl -lm -lpthread"
source="$repo/tests/cairo_scaled_image_test.zi"
image_ziran ir --define POSIX_THREADS --root "$repo/tests" --module-path "$repo/src/backend" \
    $kryon_ui_args --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$work/ir/cairo_scaled_image_test.zir; root=$work/ir; fi
    output=$work/c-$form
    image_ziran build --define POSIX_THREADS --root "$root" --module-path "$repo/src/backend" \
        $kryon_ui_args --module-path "$ziran_root/std" \
        --target=c --no-main -o "$output" "$input"
    printf '#include "cairo_scaled_image_test.h"\nint main(int argc, char **argv) { return cairo_scaled_image_test_main(argc, (uint8_t **)argv); }\n' > "$output/entry.c"
    "${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -I"$output" \
        "$output"/*.c $libs -o "$output/test"
    rm -f "$work/fixtures"/*.bgra
    "$output/test" "$work/fixtures" > "$work/$form.out"
    cat "$work/$form.out"
    python3 "$repo/tests/lanczos_goldens.py" compare "$work/fixtures" "$work/$form.out"
done
