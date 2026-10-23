#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
guest=$repo/tests/hosted_strings_guest.zi
"$ziran" ir --root "$repo/tests" -o "$work/ir" "$guest"
"$ziran" bundle --root "$repo/tests" --entry hosted_strings_guest:main \
    -o "$work/hosted-strings.zib" "$guest"
"$ziran" bundle --root "$work/ir" --entry hosted_strings_guest:main \
    -o "$work/saved.zib" "$work/ir/hosted_strings_guest.zir"
cmp "$work/hosted-strings.zib" "$work/saved.zib"
for target in c cpp; do
    output=$work/$target
    "$ziran" build --target="$target" --no-main --entry ziran_hosted_strings_test:main \
        --root "$repo/tests" --module-path "$repo/src/backend" \
        --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
        -o "$output" "$repo/tests/ziran_hosted_strings_test.zi"
    if test "$target" = c; then
        compiler=${CC:-cc}; standard=c11; suffix=c
    else
        compiler=${CXX:-c++}; standard=c++17; suffix=cpp
    fi
    "$compiler" -O2 -std="$standard" -ffunction-sections -fdata-sections \
        -I"$output" -I"$ziran_root/include" "$output"/*."$suffix" \
        "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections -lm -lpthread -o "$work/run"
    (cd "$work" && GLIBC_TUNABLES=glibc.malloc.tcache_count=0 MALLOC_PERTURB_=165 ./run)
done
echo 'Hosted strings and record fields survive shared host buffers, subsequent calls and instance runs'
