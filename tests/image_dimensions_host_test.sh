#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/image-dimensions-host"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
set -- --bind image_raster:ImageWidth=pixmap:ImageWidth --bind image_raster:ImageHeight=pixmap:ImageHeight
"$ziran" ir $kryon_project_args --root "$repo/tests" --entry image_dimensions_host_test:main "$@" -o "$out/ir" "$repo/tests/image_dimensions_host_test.zi"
for form in source saved; do
    input="$repo/tests/image_dimensions_host_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$out/ir/image_dimensions_host_test.zir"; root="$out/ir"; fi
    output="$out/$form"
    if test "$form" = source; then
        "$ziran" build $kryon_project_args --target=c --root "$root" --entry image_dimensions_host_test:main "$@" --prune-stale -o "$output" "$input"
    else
        "$ziran" build $kryon_project_args --target=c --root "$root" --entry image_dimensions_host_test:main --prune-stale -o "$output" "$input"
    fi
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections -ldl -lm -lpthread -o "$output/test"
    "$output/test"
done
# Removing those bindings reproduces the consumers' missing native symbols.
output="$out/unbound"
cat > "$out/unbound.zi" <<'ZI'
Metrics :: #import "image_raster";
#program_export
main :: () -> s32 { return Metrics.AssetWidth("absent") + Metrics.AssetHeight("absent") }
ZI
"$ziran" build $kryon_project_args --target=c --root "$out" --entry unbound:main --prune-stale -o "$output" "$out/unbound.zi"
includes=$(find "$output" -type d -printf ' -iquote %p')
# shellcheck disable=SC2046,SC2086
if "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
    $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections -ldl -lm -lpthread -o "$output/test" >"$out/unbound.log" 2>&1; then
    echo 'Expected an unbound image host to fail its native link' >&2
    exit 1
fi
rg -q 'undefined reference.*ImageWidth|undefined symbol.*ImageWidth' "$out/unbound.log"
rg -q 'undefined reference.*ImageHeight|undefined symbol.*ImageHeight' "$out/unbound.log"
echo 'Unbound image dimensions reproduced the missing ImageWidth/ImageHeight link failure'
