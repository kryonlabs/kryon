"""Display-free native VM/raster gate, also runnable on a Mac CI worker."""
import argparse
import json
import os
import platform
import shutil
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project
from raylib_project_test import solid_png

parser = argparse.ArgumentParser()
parser.add_argument('--gallery-index', type=Path)
args = parser.parse_args()
output = ROOT / 'build/scratch/zx-headless'
guest_project = output / 'app-project'
package_id = kryon_test_project(guest_project)
app = guest_project / 'src'
(output / 'assets').mkdir(exist_ok=True)
solid_png(output / 'assets/red.png', 32, 32, (230, 30, 40, 255))
(app / 'app.zi').write_text((ROOT / 'tests/zx/app.zi').read_text())
env = {**os.environ, 'ZIRAN_ROOT': str(ZIRAN_ROOT)}
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(key, None)
paths = ['--project']
bundle = output / 'app.zx'
subprocess.run([str(ZIRAN), 'bundle', '--bind', package_id + '_frame_entry:ProjectFrame=app:Frame', '--root', str(app), *paths, '--entry', package_id + '_zx_run:main',
                '--asset-dir', 'assets=' + str(output / 'assets'), '-o', str(bundle),
                str(ROOT / 'src/backend/zx_run.zi'), str(app / 'app.zi')], cwd=guest_project, env=env, check=True, timeout=120)
generated = output / 'host'
shutil.rmtree(generated, ignore_errors=True)
defines = []
if sys.platform == 'darwin':
    defines = ['--define', '__APPLE__', '--define',
               '__x86_64__' if platform.machine() == 'x86_64' else '__aarch64__']
subprocess.run([str(ZIRAN), 'build', '--root', str(ROOT / 'tests/zx'), *paths, *defines,
                '--target=c', '--no-main', '--entry', 'headless:main', '-o', str(generated),
                str(ROOT / 'tests/zx/headless.zi')], env=env, check=True, timeout=180)
program = output / 'headless'
runtime = ROOT / 'build/zx-raylib/runtime/libziran.a'
subprocess.run([env.get('CC', 'cc'), '-O2', '-Wno-main', '-ffunction-sections', '-fdata-sections',
                '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                '-I' + str(ZIRAN_ROOT / 'include'),
                *map(str, sorted(generated.glob('*.c'))), str(runtime), '-lm', '-lz', '-lpthread',
                *([] if sys.platform == 'darwin' else ['-ldl']), '-o', str(program)],
               env=env, check=True, timeout=180)
result = subprocess.run([str(program), str(bundle), 'interaction'], env=env, check=True,
                        capture_output=True, text=True, timeout=60)
assert 'zx interaction passed' in result.stdout, result.stdout + result.stderr
print(result.stdout, end='')
# Exercise the actual VM/native-editor exchange through source and saved IR.
editor_root = output / 'editor-app'
editor_root.mkdir(exist_ok=True)
(editor_root / 'app.zi').write_text((ROOT / 'tests/zx/editor_app.zi').read_text())
editor_bundle = output / 'editor.zx'
subprocess.run([str(ZIRAN), 'bundle', '--bind', 'frame_entry:ProjectFrame=app:Frame', '--root', str(editor_root), *paths,
                '--entry', 'zx_run:main', '-o', str(editor_bundle),
                str(ROOT / 'src/backend/zx_run.zi'), str(editor_root / 'app.zi')], env=env, check=True, timeout=120)
editor_ir = output / 'editor-ir'
editor_c = output / 'editor-c'
editor_program = output / 'editor'
for saved in (False, True):
    shutil.rmtree(editor_c, ignore_errors=True)
    if saved:
        shutil.rmtree(editor_ir, ignore_errors=True)
        subprocess.run([str(ZIRAN), 'ir', '--root', str(ROOT / 'tests/zx'), *paths,
                        *defines, '-o', str(editor_ir), str(ROOT / 'tests/zx/editor_host.zi')],
                       env=env, check=True, timeout=180)
        subprocess.run([str(ZIRAN_ROOT / 'build/bin/zi2c'), '--no-main', '--root', str(editor_ir),
                        *defines, '-o', str(editor_c), *map(str, sorted(editor_ir.glob('*.zir')))],
                       env=env, check=True, timeout=180)
    else:
        subprocess.run([str(ZIRAN), 'build', '--root', str(ROOT / 'tests/zx'), *paths,
                        *defines, '--target=c', '--no-main', '--entry', 'editor_host:main',
                        '-o', str(editor_c), str(ROOT / 'tests/zx/editor_host.zi')],
                       env=env, check=True, timeout=180)
    subprocess.run([env.get('CC', 'cc'), '-O2', '-Wno-main', '-ffunction-sections', '-fdata-sections',
                    '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                    '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(editor_c),
                    *map(str, sorted(editor_c.glob('*.c'))), str(runtime), '-lm', '-lz', '-lpthread',
                    *([] if sys.platform == 'darwin' else ['-ldl']), '-o', str(editor_program)],
                   env=env, check=True, timeout=180)
    subprocess.run([str(editor_program), str(editor_bundle)], env=env, check=True, timeout=60)
print('Source and saved-IR native VM editor exchange passed', flush=True)
if args.gallery_index:
    entries = json.loads(args.gallery_index.read_text())
    for entry in entries:
        subprocess.run([str(program), str(args.gallery_index.parent / entry['bundle'])],
                       env=env, check=True, timeout=60)
    print(f'All {len(entries)} full gallery ZXs executed without a display')
