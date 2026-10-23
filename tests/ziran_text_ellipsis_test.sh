#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/test/text_ellipsis.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_ellipsis_test.zi

widget_host=ziran_widget_noop_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
