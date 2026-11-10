#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/native-asset-image"
mkdir -p "$work/assets/icons" "$work/neutral"
cp "$repo/icons/ui.webp" "$work/assets/icons/ui.webp"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER ICEAUTHORITY
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/native_asset_image_test.zi"
rm -rf "$work/ir"
"$ziran" ir --project --define POSIX_THREADS --root "$repo/tests" --module-path "$repo/tests" -o "$work/ir" "$source"
for form in source saved; do
 input=$source; root=$repo/tests; paths="--project --module-path $repo/tests"
 if test "$form" = saved; then input=$work/ir/native_asset_image_test.zir; root=$work/ir; paths=""; fi
 output=$work/$form
 rm -rf "$output"
 "$ziran" build $paths --define POSIX_THREADS --root "$root" --target=c --entry native_asset_image_test:main -o "$output" "$input"
 rm -f "$output/entry.c"
 "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$output" "$output"/*.c \
  $(pkg-config --libs cairo freetype2 pangocairo fontconfig glib-2.0) -Wl,--gc-sections -ldl -lm -lpthread -o "$output/probe"
 sh "$repo/scripts/copy-native-assets.sh" "$work/assets" "$output/probe.assets" assets
 (cd "$work/neutral" && "$output/probe")
 mkdir -p "$output/prefix/bin"
 cp "$output/probe" "$output/prefix/bin/probe"
 sh "$repo/scripts/copy-native-assets.sh" "$work/assets" "$output/prefix/share/kryon/probe/assets" assets
 (cd "$work/neutral" && "$output/prefix/bin/probe")
done
