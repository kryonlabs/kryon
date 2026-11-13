#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
    --define KRYON_HOSTED_UI --entry hosted_action_modal_guest:main \
    -o "$work/hosted-action-modal.zx" "$repo/tests/hosted_action_modal_guest.zi"
"$ziran" build --target=c --no-main --entry hosted_action_modal_host_test:main \
    --root "$repo/tests" $kryon_project_args -o "$work/c" \
    "$repo/tests/hosted_action_modal_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c \
    "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections \
    -lm -lpthread -o "$work/run"
(cd "$work" && ./run)
echo 'A package consumer action dialog passes through the real widget host'
