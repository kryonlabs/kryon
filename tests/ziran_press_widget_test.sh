#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/press-widget.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry ziran_press_widget_test:Answer \
    -o "$work/test.zib" "$repo/tests/ziran_press_widget_test.zi"
test "$("$ziran" run "$work/test.zib")" = 42
"$ziran" build --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --target=c \
    --entry ziran_press_widget_test:main -o "$work/c" "$repo/tests/ziran_press_widget_test.zi"
"${CC:-cc}" -std=c11 -O1 -I"$work/c" "$work/c"/*.c -o "$work/c/run"
"$work/c/run"
