#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/android-text-editor-test
source=$repo/tests/android_text_editor_behavior.zi
mkdir -p "$work"

# The device protocol compiles exactly as it does for the APK, with
# ANDROID_BUILD active; the raylib JNI surface stays stubbed by the host
# file, and bionic's zero-initialized mutexes stand in as glibc ones.
"$ziran_root/build/bin/zi2c" --no-main --define ANDROID_BUILD \
    --root "$repo" --module-path "$ziran_root/std" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    -o "$work/c" "$source"

includes="-I$ziran_root/include -I$work/c"
for header in $(find "$work/c" -name '*.h' | xargs -n1 dirname | sort -u); do
    includes="$includes -I$header"
done
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections $includes $(find "$work/c" -name '*.c') \
    -Wl,--gc-sections -o "$work/test" -pthread -ldl
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
printf '%s\n' 'Android input method snapshots, revisions, field identity and blur passed'
