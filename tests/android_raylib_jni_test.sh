#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
work="$repo/build/scratch/android-raylib-jni"
mkdir -p "$work"
for route in source ir; do
    output="$work/$route"
    mkdir -p "$output"
    input="$repo/tests/android_raylib_jni_test.zi"
    if [ "$route" = ir ]; then
        "$ziran" ir --define ANDROID_BUILD --define KRYON_RAYLIB_HOST --root "$repo/tests" \
            $kryon_project_args -o "$work/test.zir" "$input"
        input="$work/test.zir/android_raylib_jni_test.zir"
    fi
    "$ziran" build --define ANDROID_BUILD --define KRYON_RAYLIB_HOST --target=c --no-main \
        --entry android_raylib_jni_test:Check --root "$repo/tests" $kryon_project_args \
        -o "$output" "$input"
    cat > "$output/main.c" <<'C'
#include <stdio.h>
#include "android_raylib_jni_test.h"
int main(void) {
    int result = Check();
    if (result) fprintf(stderr, "Android raylib JNI test failed: %d\n", result);
    return result;
}
C
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" \
        -Wl,--gc-sections -lm -lpthread -ldl -o "$output/test"
    "$output/test"
done
echo 'Android raylib JNI: selected NativeActivity, private files and pending/active surfaces and resumed framebuffer passed (source and saved IR)'
