#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work="$repo/build/scratch/android-pipeline/test"
mkdir -p "$work"
for route in source ir; do
    output="$work/$route"
    mkdir -p "$output"
    input="$repo/tests/render_pipeline_test.zi"
    if [ "$route" = ir ]; then
        "$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
            --module-path "$repo/src/ui" -o "$work/test.zir" "$input"
        input="$work/test.zir/render_pipeline_test.zir"
    fi
    "$ziran" build --target=c --no-main --entry render_pipeline_test:Check \
        --root "$repo/tests" --module-path "$repo/src/backend" \
        --module-path "$repo/src/ui" -o "$output" "$input"
    cat > "$output/main.c" <<'C'
#include <stdio.h>
#include "render_pipeline_test.h"
int main(void) {
    int result = Check();
    if (result) fprintf(stderr, "render pipeline test failed: %d\n", result);
    return result;
}
C
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" \
        -lm -lpthread -o "$output/test"
    timeout 30s "$output/test"
done
echo 'Local render pipeline tests: owned text/assets/meshes, overlapping frames, render-owned resource calls, all paint flush paths and worker reuse passed (C source and saved IR)'
