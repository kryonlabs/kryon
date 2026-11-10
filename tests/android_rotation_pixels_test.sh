#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
work=$repo/build/scratch/android-rotation
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
source=$repo/tests/android_rotation_pixels.zi
cd "$repo"
for platform in portable android; do
    define=
    if test "$platform" = android; then
        # Select the native Android surface branch through its supported flag.
        # The fixture executes on Linux, so standard threads retain real glibc
        # completion polling. Separate NDK gates cover ANDROID_BUILD/Bionic.
        define='--define PLATFORM_ANDROID'
    fi
    ir=$work/$platform-ir
    rm -rf "$ir"
    "$ziran" ir --project --module-path "$repo/tests" $define --root "$repo/tests" -o "$ir" "$source"
    for form in source saved; do
        input=$source
        args="--project --module-path $repo/tests --root $repo/tests"
        if test "$form" = saved; then
            input=$ir/android_rotation_pixels.zir
            args="--root $ir --module-path $ir"
        fi
        if test "$platform" = portable; then
            "$ziran" bundle $args --entry android_rotation_pixels:main -o "$work/$form.zx" "$input"
            result=$("$ziran" run "$work/$form.zx")
            test "$result" = 0
        fi
        output=$work/$platform-$form-c
        rm -rf "$output"
        "$ziran" build --target=c $define $args --entry android_rotation_pixels:main -o "$output" "$input"
        "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -I"$output" -I"$ziran_root/include" "$output"/*.c \
            "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -lm -lpthread -o "$output/test"
        "$output/test"
    done
    if test "$platform" = portable; then cmp "$work/source.zx" "$work/saved.zx"; fi
done
echo 'Android portrait/landscape/portrait: current native surface transform and actual Pixmap circle/text pixels passed source/saved C; portable policy/raster source/saved ZX match and execute'
