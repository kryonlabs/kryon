#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_press_widget_test.zi

set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_keyboard_navigation_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_keyboard_navigation_host:RasterRoundedRectangleOutline

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
