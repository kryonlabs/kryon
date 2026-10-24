#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_modal_layer_test.zi
set --
matrix_forms=both matrix_c=cc matrix_bundles=after-native matrix_std="$ziran_root/std" . "$repo/tests/main_matrix.sh"
echo 'Modal input transactions passed native and portable execution'
