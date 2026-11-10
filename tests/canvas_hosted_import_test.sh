#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
work=$repo/build/scratch/canvas-hosted-import
mkdir -p "$work"
project=$work/project
python3 - "$repo" "$project" <<'PYFIXTURE'
from pathlib import Path
import sys
repo, project = map(Path, sys.argv[1:])
sys.path.insert(0, str(repo / "tests"))
from toolchain import kryon_test_project
kryon_test_project(project)
(project / "src/canvas_hosted_import.zi").write_bytes((repo / "tests/canvas_hosted_import.zi").read_bytes())
PYFIXTURE
cd "$project"

for mode in hosted native; do
    defines="--define PLATFORM_WEB"
    if test "$mode" = hosted; then
        defines="--define KRYON_HOSTED_UI --define PLATFORM_WEB"
    fi
    # shellcheck disable=SC2086
    "$ziran" bundle --project --offline --root "$project/src" \
        $defines --entry canvas_hosted_import:Probe \
        -o "$work/$mode.zx" "$project/src/canvas_hosted_import.zi"
    test "$("$ziran" run "$work/$mode.zx")" = 0
done

printf '%s\n' 'Canvas hosted/native module imports passed'
