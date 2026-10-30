#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work="$repo/build/scratch/bundle-preload"
rm -rf "$work"
mkdir -p "$work/c"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    -o "$work/ir" "$repo/tests/ziran_bundle_names_test.zi"
"$ziran" bundle --root "$work/ir" --entry ziran_bundle_names_test:main \
    -o "$work/fixture.zx" "$work/ir/ziran_bundle_names_test.zir"
"$ziran" build --target=c --no-main --entry bundle_preload_test:Check --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/bundle_preload_test.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include <stdlib.h>
#include "bundle_preload_test.h"
int main(int argc, char **argv) {
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (!file) return 20;
    fseek(file, 0, SEEK_END); long size = ftell(file); fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) return 21;
    fclose(file);
    int result = Check(data, size);
    if (result) fprintf(stderr, "bundle preload check %d failed\n", result);
    free(data);
    return result;
}
C
includes=$(find "$work/c" -type d -printf ' -iquote %p')
# shellcheck disable=SC2046,SC2086
"${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" $includes $(find "$work/c" -name '*.c') \
    "$ziran_root/build/libziran.a" -lm -lpthread -o "$work/test"
"$work/test" "$work/fixture.zx"
echo 'Bundle preloads: concurrent opens match a direct open; mismatched, invalid and discarded preloads stay safe'
