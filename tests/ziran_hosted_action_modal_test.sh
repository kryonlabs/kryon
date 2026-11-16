#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/hosted-action-modal.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
# Compile the guest as a separate consumer so its imported modules carry a
# package identity, as they do in applications.
python3 - "$repo" "$work/project" <<'PYFIXTURE'
from pathlib import Path
import sys
repo, project = map(Path, sys.argv[1:])
sys.path.insert(0, str(repo / 'tests'))
from toolchain import kryon_test_project
kryon_test_project(project)
(project / 'src/hosted_action_modal_guest.zi').write_bytes(
    (repo / 'tests/hosted_action_modal_guest.zi').read_bytes())
PYFIXTURE
(cd "$work/project" && "$ziran" bundle --root "$work/project/src" --project --offline \
    --define KRYON_HOSTED_UI --entry hosted_action_modal_guest:main \
    -o "$work/hosted-action-modal.zx" "$work/project/src/hosted_action_modal_guest.zi")
"$ziran" build --target=c --no-main --entry hosted_action_modal_host_test:main \
    --root "$repo/tests" $kryon_project_args -o "$work/c" \
    "$repo/tests/hosted_action_modal_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c \
    "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections \
    -lm -lpthread -o "$work/run"
(cd "$work" && ./run)
echo 'A package consumer action dialog passes through the real widget host'
