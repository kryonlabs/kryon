#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/image-preview-web"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS GDK_DISPLAY SESSION_MANAGER
export ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0

"$ziran" ir --project --root "$repo/tests" --define PLATFORM_WEB -o "$work/ir" "$repo/tests/image_preview_web_test.zi"
for form in source saved; do
    source="$repo/tests/image_preview_web_test.zi"
    root="$repo/tests"
    if test "$form" = saved; then
        source="$work/ir/image_preview_web_test.zir"
        root="$work/ir"
    fi
    output="$work/$form"
    "$ziran" build --project --target=c --no-main --root "$root" --define PLATFORM_WEB -o "$output" "$source"
    cat > "$output/main.c" <<'C'
#include "image_preview_web_test.h"
int main(void) { return Check(); }
C
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # No native AVIF archives: this link catches accidental native imports.
    # shellcheck disable=SC2086
    "${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" $includes "$output"/*.c -lm -o "$output/test"
    "$output/test"
done
echo 'Browser image previews: source/saved RGBA pixels and unsupported encoder passed without native codecs'
