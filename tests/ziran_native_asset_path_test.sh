#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER ICEAUTHORITY
export YUE_DESKTOP_RECOVERY=0
work="$repo/build/scratch/native-asset-path"
mkdir -p "$work/asset-source/icons" "$work/neutral"
printf '%s' fixture > "$work/asset-source/icons/test.bin"
printf '%s' dynamic > "$work/neutral/dynamic.bin"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/ir" "$repo/tests/native_asset_path_test.zi"
for form in source saved; do
 input="$repo/tests/native_asset_path_test.zi"
 if test "$form" = saved; then input="$work/ir/native_asset_path_test.zir"; fi
 "$ziran" build --target=c --exe --entry native_asset_path_test:main --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/$form" "$input"
 program="$work/$form/native_asset_path_test"
 sh "$repo/scripts/copy-native-assets.sh" "$work/asset-source" "$program.assets" assets
 (cd "$work/neutral" && "$program" "$work/asset-source/icons/test.bin")
 mkdir -p "$work/$form/prefix/bin"
 cp "$program" "$work/$form/prefix/bin/probe"
 sh "$repo/scripts/copy-native-assets.sh" "$work/asset-source" "$work/$form/prefix/share/kryon/probe/assets" assets
 (cd "$work/neutral" && "$work/$form/prefix/bin/probe" "$work/asset-source/icons/test.bin")
 # Replacing a snapshot drops removed files; assetsless packaging drops only its snapshot.
 touch "$work/$form/prefix/share/kryon/probe/assets/stale"
 sh "$repo/scripts/copy-native-assets.sh" "$work/asset-source" "$work/$form/prefix/share/kryon/probe/assets" assets
 test ! -e "$work/$form/prefix/share/kryon/probe/assets/stale"
 sh "$repo/scripts/copy-native-assets.sh" '' "$work/$form/prefix/share/kryon/probe/assets" ''
 test ! -e "$work/$form/prefix/share/kryon/probe/assets"
done
