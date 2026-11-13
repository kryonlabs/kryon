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
    defines=""
    if test "$mode" = hosted; then
        defines="--define KRYON_HOSTED_UI"
    fi
    # shellcheck disable=SC2086
    "$ziran" build --project --offline --target=c --target-os web --no-main \
        --root "$project/src" $defines -o "$work/$mode-c" \
        "$project/src/canvas_hosted_import.zi"
done

printf '%s\n' 'Canvas hosted/native module imports passed'
