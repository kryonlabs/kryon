#!/bin/sh
# Warped images through the Cairo host, from C and saved IR.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/cairo-mesh"
mkdir -p "$work/fixtures"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
python3 - "$work/fixtures" <<'PY'
from pathlib import Path
from PIL import Image
import sys
image = Image.new('RGBA', (2, 2))
image.putdata([(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 255, 255)])
image.save(Path(sys.argv[1]) / 'mesh.png')
PY
libs="$(pkg-config --libs cairo freetype2 pangocairo fontconfig glib-2.0) -ldl -lm -lpthread"
source="$repo/tests/cairo_mesh_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$work/ir/cairo_mesh_test.zir; root=$work/ir; fi
    for target in c; do
        output=$work/$target-$form
        "$ziran" build --root "$root" --module-path "$repo/src/backend" \
            --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
            --target="$target" --no-main -o "$output" "$input"
        suffix=c; header=h; compiler=${CC:-cc}; standard=c11
        printf '#include "cairo_mesh_test.%s"\nint main(int argc, char **argv) { return cairo_mesh_test_main(argc, (uint8_t **)argv); }\n' "$header" > "$output/entry.$suffix"
        "$compiler" -std="$standard" -O2 -I"$ziran_root/include" -I"$output" \
            "$output"/*."$suffix" $libs -o "$output/test"
        "$output/test" "$work/fixtures"
    done
done
