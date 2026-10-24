#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_area_hidden_test.zi
set -- --bind font_metrics:MeasureGlyphWidth=ziran_text_area_cache_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_text_area_cache_host:MeasureGlyphLineHeight
matrix_forms=both matrix_c=cc matrix_bundles=after-native matrix_std="$ziran_root/std" . "$repo/tests/main_matrix.sh"
echo 'Hidden text area bytes passed native and portable execution'
