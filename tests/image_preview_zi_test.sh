#!/bin/sh
set -eu

# Gallery previews made by image_preview.zi, decoded by Pillow and compared
# with the same images turned and scaled by Pillow itself, and the Exif
# orientation reader (image_orientation.zi) on both byte orders.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/image-preview-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET XAUTHORITY DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS SESSION_MANAGER XDG_RUNTIME_DIR
export ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
artifact=$("$ziran" pkg artifact avif:native --platform linux-x86_64-gnu)
avif_native=$(dirname "$artifact")

"$ziran" build --target=c --no-main --root "$repo/tests" $kryon_project_args \
    -o "$work/c" "$repo/tests/image_preview_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include "image_preview_behavior.h"
int main(void)
{
    int result = Check();
    if (result != 0) fprintf(stderr, "image preview check %d failed\n", result);
    return result == 0 ? 0 : 1;
}
C
includes=$(find "$work/c" -type d -printf ' -iquote %p')
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" $includes "$work"/c/*.c "$avif_native/lib/libavif.a" "$avif_native/lib/libaom.a" -lpthread -ldl -lm -o "$work/test"
(cd "$work" && ./test)
python3 "$repo/tests/image_preview_check.py" "$work"
echo "Kryon image previews and Exif orientations passed"

# Exercise the same public package API with real AVIF bytes and Lanczos pixels.
sh "$repo/tests/image_avif_zi_test.sh"
