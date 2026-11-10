#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/android-picker-test
source=$repo/tests/android_picker_behavior.zi
mkdir -p "$work"
exec 9>"$work/lock"
flock 9

rm -rf "$work/c"

# The picker compiles exactly as it does for the APK, with ANDROID_BUILD
# active; a fake JNI environment in the test stands in for the activity,
# and bionic's zero-initialized mutexes stand in as glibc ones.
"$ziran" build --project --target=c --module-path "$repo/tests" --prune-stale --no-main --define ANDROID_BUILD \
    --root "$repo/tests" \
    -o "$work/c" "$source"

includes="-I$ziran_root/include -I$work/c"
for header in $(find "$work/c" -name '*.h' | xargs -n1 dirname | sort -u); do
    includes="$includes -I$header"
done
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections $includes $(find "$work/c" -name '*.c') \
    -Wl,--gc-sections -o "$work/test" -pthread -ldl -lm
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
printf '%s\n' 'Android document picker and QR scanner requests passed'
