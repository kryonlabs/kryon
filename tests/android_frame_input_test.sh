#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY KRYON_TEST_DRIVER
export YUE_DESKTOP_RECOVERY=0
# Test-owned, ignored output; never replace shared compilers or host binaries.
work="$repo/work/android-frame-input"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
webp=${WEBP_A:-"$repo/build/webp-ziran/libkryonwebp.a"}
avif=${AVIF_A:-"$repo/build/avif-ziran/avif/libavif.a"}
aom=${AOM_A:-"$repo/build/avif-ziran/aom/libaom.a"}
for path in "$webp" "$avif" "$aom"; do
    test -f "$path" || { echo "Set WEBP_A, AVIF_A and AOM_A to the existing native image archives: missing $path" >&2; exit 1; }
done
rm -f "$work/c"/*.c "$work/c"/*.h
# Deterministic Android OS calls and events; no window system is contacted.
"$ziran" build --project --entry android_frame_input_test:main --bind frame_entry:ProjectFrame=android_frame_app:Frame --target=c --define ANDROID_BUILD --root "$repo/tests" --module-path "$repo/tests/android_frame_input" \
    --module-path "$repo/tests" --no-main -o "$work/c" "$repo/tests/android_frame_input_test.zi" "$repo/tests/android_frame_input/android_frame_app.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c "$webp" "$avif" "$aom" "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
    -Wl,--gc-sections -ldl -lpthread -lm -o "$work/c/test"
(cd "$work" && timeout --kill-after=2s 45s ./c/test)
printf '%s\n' 'Android frame input: canonical host publication and revocation passed'
