#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
work=$repo/build/scratch/canvas-hosted-import
mkdir -p "$work"

for mode in hosted native; do
    defines="--define PLATFORM_WEB"
    if test "$mode" = hosted; then
        defines="--define KRYON_HOSTED_UI --define PLATFORM_WEB"
    fi
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "kryon=$repo/src/ui" \
        --module-path "kryon=$repo/src/backend" --module-path "$ziran_root/std" \
        $defines --entry canvas_hosted_import:Probe \
        -o "$work/$mode.zx" "$repo/tests/canvas_hosted_import.zi"
    test "$("$ziran" run "$work/$mode.zx")" = 0
done

printf '%s\n' 'Canvas hosted/native module imports passed'
