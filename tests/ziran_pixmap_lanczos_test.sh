#!/bin/sh
# Smooth images through the pixmap host in a native threaded build: once
# its Lanczos copy is made, a picture is drawn from a copy slot pixel for
# pixel, the same as Pillow's Image.resize(LANCZOS, box) over the frame.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-lanczos"
rm -rf "$work"
mkdir -p "$work/fixtures"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
python3 "$repo/tests/lanczos_goldens.py" fixtures "$work/fixtures"
source="$repo/tests/pixmap_lanczos_test.zi"
"$ziran" ir --project --root "$repo/tests" --define POSIX_THREADS -o "$work/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$work/ir/pixmap_lanczos_test.zir; root=$work/ir; fi
    LDLIBS="-lm -lpthread" "$ziran" build --project --root "$root" --define POSIX_THREADS --target=c \
        --entry pixmap_lanczos_test:main --exe -o "$work/c-$form" "$input"
    rm -f "$work/fixtures"/*.bgra
    "$work/c-$form/pixmap_lanczos_test" "$work/fixtures" > "$work/$form.out"
    cat "$work/$form.out"
    python3 "$repo/tests/lanczos_goldens.py" compare "$work/fixtures" "$work/$form.out"
done
