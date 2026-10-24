#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work="$repo/build/scratch/android-pipeline/jni-test"
mkdir -p "$work"
for route in source ir; do
    output="$work/$route"
    mkdir -p "$output"
    input="$repo/tests/android_jni_threads_test.zi"
    if [ "$route" = ir ]; then
        "$ziran" ir --define ANDROID_BUILD --define __x86_64__ --root "$repo/tests" --module-path "$repo/src/backend" \
            --module-path "$repo/src/ui" -o "$work/test.zir" "$input"
        input="$work/test.zir/android_jni_threads_test.zir"
    fi
    "$ziran" build --define ANDROID_BUILD --define __x86_64__ --target=c --no-main --entry android_jni_threads_test:Check \
        --root "$repo/tests" --module-path "$repo/src/backend" \
        --module-path "$repo/src/ui" -o "$output" "$input"
    cat > "$output/main.c" <<'C'
#include <stdio.h>
#include "android_jni_threads_test.h"
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
echo 'Local JNI thread tests: independent render/app attachments, local frame balance, explicit cleanup and thread-exit cleanup passed (C source and saved IR)'
