#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export YUE_DESKTOP_RECOVERY=0
# Test-owned, ignored output; never replace shared compilers or host binaries.
work="$repo/work/frame-input"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
source="$repo/tests/frame_snapshot_test.zi"
set -- --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std"
"$ziran" ir "$@" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input="$work/ir/frame_snapshot_test.zir"; fi
    "$ziran" bundle "$@" --entry frame_snapshot_test:main -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 0
    "$ziran" build --target=c "$@" --entry frame_snapshot_test:main -o "$work/$form-c" "$input"
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$work/$form-c" \
        "$work/$form-c"/*.c -Wl,--gc-sections -lm -o "$work/$form-c/test"
    "$work/$form-c/test"
done
printf '%s\n' 'Frame snapshot ownership and session/legacy-input isolation: source/saved C and VM passed'
