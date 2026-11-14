#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
work="$repo/build/scratch/hosted-box-host"
mkdir -p "$work" "$work/c"

"$ziran" bundle --root "$repo/tests" $kryon_project_args \
    --define KRYON_HOSTED_UI --entry hosted_box_guest:main \
    -o "$work/hosted-box.zx" "$repo/tests/hosted_box_guest.zi"
"$ziran" build --target=c --no-main --entry hosted_box_host_test:main \
    --root "$repo/tests" $kryon_project_args -o "$work/c" \
    "$repo/tests/hosted_box_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c \
    "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections \
    -lm -lpthread -o "$work/run"
(cd "$work" && ./run)
printf '%s\n' 'Public Box.Draw passed through a real portable HostedApp host capability'
