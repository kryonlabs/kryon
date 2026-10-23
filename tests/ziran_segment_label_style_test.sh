#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/test/segment_label_style.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_segment_label_style_test.zi

widget_host=ziran_widget_noop_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_widget_noop_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_widget_noop_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
