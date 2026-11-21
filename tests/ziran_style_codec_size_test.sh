#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export HARMONY_DESKTOP_RECOVERY=0
mkdir -p "$repo/build/scratch/style-codec-size"
work=$(mktemp -d "$repo/build/scratch/style-codec-size/run.XXXXXX")
trap 'rm -rf -- "$work"' EXIT HUP INT TERM
source=$repo/tests/style_codec_size_test.zi
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    if [ "$form" = saved ]; then input=$work/ir/style_codec_size_test.zir; root=$work/ir; fi
    "$ziran" build --project --root "$root" --target=c --exe \
        --entry style_codec_size_test:Main -o "$work/$form" "$input"
    "$work/$form/style_codec_size_test"
done
printf 'Style size host codecs: native source/saved IR passed.\n'
