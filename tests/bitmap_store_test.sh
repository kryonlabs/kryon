#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/bitmap-store"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/bitmap_store_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args --module-path "$ziran_root/std" -o "$out/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$out/ir/bitmap_store_test.zir; root=$out/ir; fi
    "$ziran" build --target=c --root "$root" $kryon_ui_args --module-path "$ziran_root/std" --entry bitmap_store_test:main --exe -o "$out/$form" "$input"
    "$out/$form/bitmap_store_test"
    "$ziran" bundle --root "$root" $kryon_ui_args --module-path "$ziran_root/std" --entry bitmap_store_test:main -o "$out/$form.zx" "$input"
    result=$("$ziran" run "$out/$form.zx")
    printf '%s\n' "$result"
    test "$(printf '%s\n' "$result" | tail -1)" = 0
done

"$ziran" build --target=c --root "$repo/tests" $kryon_ui_args --module-path "$ziran_root/std" --entry bitmap_budget_test:main --exe -o "$out/budget" "$repo/tests/bitmap_budget_test.zi"
"$out/budget/bitmap_budget_test"
