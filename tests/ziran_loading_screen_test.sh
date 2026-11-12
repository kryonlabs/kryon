#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/loading-screen/test"
mkdir -p "$work"
"$ziran" build --target=c --exe --root "$repo/tests" $kryon_project_args \
    --entry loading_screen_layout_test:main -o "$work/c" "$repo/tests/loading_screen_layout_test.zi"
"$work/c/loading_screen_layout_test"
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
    --entry loading_screen_layout_test:main -o "$work/layout.zx" "$repo/tests/loading_screen_layout_test.zi"
"$ziran" run "$work/layout.zx"
"$ziran" build --define KRYON_HOSTED_UI --target=c --root "$repo/src/ui" $kryon_project_args \
    -o "$work/hosted" "$repo/src/ui/loadingscreen/loadingscreen.zi"
