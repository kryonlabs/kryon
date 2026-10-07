#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$repo/build/scratch/system-typeface-android
mkdir -p "$work"
source=$repo/tests/system_typeface_android_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/system_typeface_android_test.zir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$output" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c11; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -O1 -I"$output" "$output"/*."$extension" -Wl,--wrap=dlopen -Wl,--wrap=dlsym -ldl -o "$output/test"
        "$output/test"
        KRYON_TEST_OLD_ANDROID=1 "$output/test"
    done
done
echo 'Android font selection, UTF-16, collection faces, cleanup and older API fallback passed C/C++ source and saved IR'
