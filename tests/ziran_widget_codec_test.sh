#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
mkdir -p "$repo/build/scratch/widget-codec-test"
work=$(mktemp -d "$repo/build/scratch/widget-codec-test/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
python3 "$repo/tests/widget_codec_generator_test.py" "$repo" "$work" "$ziran_root"
