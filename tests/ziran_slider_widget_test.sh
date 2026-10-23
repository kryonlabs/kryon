#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/slider-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_slider_widget_test.zi

widget_host=ziran_slider_widget_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
