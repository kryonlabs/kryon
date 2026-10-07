#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
build=$repo/build/accessibility
mkdir -p "$build"
exec 9>"$build/lock"
flock 9
"$ziran" ir --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --root "$repo/tests" -o "$build/ir" "$repo/tests/atspi_fixture.zi"
for source in source saved; do
    input=$repo/tests/atspi_fixture.zi
    root=$repo/tests
    if [ "$source" = saved ]; then input=$build/ir/atspi_fixture.zir; root=$build/ir; fi
    output=$build/$source-c
    "$ziran" build --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --target=c --no-main --root "$root" \
        --module-path "$build/ir" --entry atspi_fixture:Run -o "$output" "$input"
    printf '#include "atspi_fixture.h"\nint main(void) { return Run(); }\n' >"$output/driver.c"
    "${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -I"$output" \
        "$output"/*.c $(pkg-config --libs gio-2.0) -lm -lpthread -o "$output/run"
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
        YUE_DESKTOP_RECOVERY=0 xvfb-run -a dbus-run-session -- \
        /usr/bin/python3 "$repo/tests/accessibility_atspi_test.py" "$output/run"
done
