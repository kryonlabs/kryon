#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
std="${include%/include}/std"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_dropdown_widget_test.zi

widget_host=ziran_dropdown_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_dropdown_widget_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_dropdown_widget_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$std"
. "$repo/tests/widget_matrix.sh"
