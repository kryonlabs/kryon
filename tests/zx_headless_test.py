"""Display-free native VM/raster gate, also runnable on a Mac CI worker."""
import argparse
import json
import os
import platform
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from raylib_project_test import solid_png

parser = argparse.ArgumentParser()
parser.add_argument('--gallery-index', type=Path)
args = parser.parse_args()
output = ROOT / 'build/scratch/zx-headless'
app = output / 'app'
app.mkdir(parents=True, exist_ok=True)
(output / 'assets').mkdir(exist_ok=True)
solid_png(output / 'assets/red.png', 32, 32, (230, 30, 40, 255))
(app / 'app.zi').write_text((ROOT / 'tests/zx/app.zi').read_text())
env = {**os.environ, 'ZIRAN_ROOT': str(ZIRAN_ROOT)}
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(key, None)
paths = ['--module-path', str(ROOT / 'src/ui'), '--module-path', str(ROOT / 'src/backend'),
         '--module-path', 'kryon=' + str(ROOT / 'src/ui'),
         '--module-path', 'kryon=' + str(ROOT / 'src/backend'),
         '--module-path', str(ZIRAN_ROOT / 'std')]
bundle = output / 'app.zx'
subprocess.run([str(ZIRAN), 'bundle', '--root', str(app), *paths, '--entry', 'zx_run:main',
                '--asset-dir', 'assets=' + str(output / 'assets'), '-o', str(bundle),
                str(ROOT / 'src/backend/zx_run.zi')], env=env, check=True, timeout=120)
generated = output / 'host'
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
if args.gallery_index:
    entries = json.loads(args.gallery_index.read_text())
    for entry in entries:
        subprocess.run([str(program), str(args.gallery_index.parent / entry['bundle'])],
                       env=env, check=True, timeout=60)
    print(f'All {len(entries)} full gallery ZXs executed without a display')
