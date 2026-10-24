#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
std="${include%/include}/std"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_frame_retained_test.zi

set -- \
    --bind raster:RasterLine=ziran_frame_retained_host:RasterLine \
    --bind raster_text:RasterText=ziran_frame_retained_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_frame_retained_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_frame_retained_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh

matrix_bundles=before
matrix_forms=both
matrix_c=driver
matrix_std="$std"
. "$repo/tests/main_matrix.sh"
