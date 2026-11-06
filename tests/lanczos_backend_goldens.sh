#!/bin/sh
# Smooth images on every backend draw their shared Lanczos copies the same
# as Pillow's Image.resize(LANCZOS, box): Cairo and pixmap natively, Raylib
# in a private Xvfb window and Canvas in private headless Firefox.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
sh "$repo/tests/ziran_cairo_scaled_image_test.sh"
sh "$repo/tests/ziran_pixmap_lanczos_test.sh"
sh "$repo/tests/raylib_lanczos_test.sh"
python3 "$repo/tests/canvas_lanczos_test.py"
echo "Lanczos goldens passed on Cairo, pixmap, Raylib and Canvas"
