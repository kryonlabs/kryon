#!/bin/sh
# Invoke through build-queue. Real package exports must survive competing app roots.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/scratch/widget-import-order
mkdir -p "$work"
mkdir "$work/.running"
trap 'rmdir "$work/.running"' EXIT HUP INT TERM
python3 - "$repo" "$work" <<'PY'
from pathlib import Path
import subprocess
import sys
repo, work = map(Path, sys.argv[1:])
sys.path.insert(0, str(repo / 'tests'))
from toolchain import ZIRAN, kryon_test_project
project = work / 'project'
kryon_test_project(project)
competitor = project / 'competing'
competitor.mkdir(exist_ok=True)
(competitor / 'navigation.zi').write_bytes((repo / 'tests/fixtures/import_collision/navigation.zi').read_bytes())
source = (repo / 'tests/widget_import_order_test.zi').read_text()
standard = next(line for line in source.splitlines(True) if line.startswith('StdText ::'))
without_standard = source.replace(standard, '', 1)
manifest = (project / 'ziran.toml').read_text()
failed = False
for roots_name, roots in [('src-first', '["src", "competing"]'), ('competitor-first', '["competing", "src"]')]:
    (project / 'ziran.toml').write_text(manifest.replace('module_roots = ["src"]', 'module_roots = ' + roots))
    subprocess.run([str(ZIRAN), 'lock', '--offline'], cwd=project, check=True)
    for imports_name, fixture in [('std-first', standard + without_standard), ('widgets-first', without_standard + standard)]:
        (project / 'src/widget_import_order_test.zi').write_text(fixture)
        result = subprocess.run([str(ZIRAN), 'check', '--project', '--offline', 'src/widget_import_order_test.zi'], cwd=project)
        failed |= result.returncode != 0
        print('Package Text/Image/NavigationBar and std/text: ' + roots_name + '/' + imports_name + ' code=' + str(result.returncode), flush=True)
raise SystemExit(int(failed))
PY
