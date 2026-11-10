#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
avif=${AVIF_PACKAGE_ROOT:-$("$ziran" pkg path avif)}
foundation=${IMAGE_CODEC_ROOT:-$("$ziran" pkg path image_codec)}
artifact=${AVIF_NATIVE_ARTIFACT:-$(cat "$avif/build/scratch/avif/native-artifact.txt")}
base=$(dirname "$artifact")
export LDLIBS="$base/lib/libavif.a $base/lib/libaom.a -lpthread -ldl -lm"
out="$repo/build/scratch/image-avif"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET XAUTHORITY DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS SESSION_MANAGER XDG_RUNTIME_DIR
export ZIRAN_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
set -- --root "$repo/tests" --module-path "$repo/src/backend" --module-path "avif=$avif/src" --module-path "$avif/src" --module-path "$avif/src/bridge" --module-path "image_codec=$foundation/src" --module-path "std=$ziran_root/std"
"$ziran" ir "$@" -o "$out/ir" "$repo/tests/image_avif_behavior.zi"
for form in source saved; do
    input="$repo/tests/image_avif_behavior.zi"
    if test "$form" = saved; then input="$out/ir/image_avif_behavior.zir"; set -- --root "$out/ir" --module-path "$out/ir"; fi
    "$ziran" build --target=c --exe --entry image_avif_behavior:main "$@" -o "$out/$form" "$input"
    "$out/$form/image_avif_behavior"
done
