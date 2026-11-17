#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/cairo-picture-cache"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
"$ziran" ir $kryon_project_args --root "$repo/tests" --define POSIX_THREADS -o "$work/ir" "$repo/tests/cairo_picture_cache_test.zi"
for form in source saved; do
    input="$repo/tests/cairo_picture_cache_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$work/ir/cairo_picture_cache_test.zir"; root="$work/ir"; fi
    output="$work/$form"
    "$ziran" build $kryon_project_args --target=c --root "$root" --define POSIX_THREADS --prune-stale -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections \
        $(pkg-config --libs cairo freetype2 fontconfig pangocairo) -ldl -lm -lpthread -o "$output/test"
    python3 - "$work" <<'PY'
from pathlib import Path
from PIL import Image
import shutil, sys
root = Path(sys.argv[1])
Image.new('RGBA', (1, 1), (208, 16, 32, 255)).save(root / 'red.png')
Image.new('RGBA', (3, 2), (18, 100, 218, 255)).save(root / 'blue.png')
for folder in ('icons', 'icons/sub', 'icons-old'):
    (root / folder).mkdir(exist_ok=True)
    shutil.copyfile(root / 'red.png', root / folder / 'pic.png')
PY
    (cd "$work" && "$output/test")
done
