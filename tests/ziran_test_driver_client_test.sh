#!/bin/sh
# A test process's client for the test driver finds, presses, types, sends
# keys and waits for settled frames, natively in C.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/ziran_test_driver_client_test.zi"
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$work/c" "$work/c"/*.c -lm \
    -o "$work/c/ziran_test_driver_client_test"
mkdir "$work/driver"
status=0
KRYON_TEST_DRIVER="$work/driver" "$work/c/ziran_test_driver_client_test" || status=$?
if [ "$status" -ne 0 ]; then
    echo "test driver client failed with $status" >&2
    exit 1
fi
echo 'Test driver client finds, presses, types, sends keys and waits for settled frames'
