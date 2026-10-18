"""Render every exported gallery bundle using only an owned private X server."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT
from raylib_project_test import png_pixels, rgb

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--player', type=Path, default=ROOT / 'build/bin/zib')
parser.add_argument('--output', type=Path, default=ROOT / 'build/gallery-zib')
parser.add_argument('--screenshots', default='screenshots')
parser.add_argument('--runner', default='')
parser.add_argument('--private-display', action='store_true')
args = parser.parse_args()
env = os.environ.copy()
if not args.private_display:
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(name, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    env['KRYON_FONT_PATH'] = str(ROOT / 'assets/fonts/LiberationSans-Regular.ttf')
    if args.runner == 'wine':
        env['WINEPREFIX'] = str(args.output.resolve() / 'wine')
        env['WINEDEBUG'] = '-all'
    subprocess.run(['xvfb-run', '-a', sys.executable, __file__, '--private-display',
                    '--player', str(args.player.resolve()), '--output', str(args.output.resolve()),
                    '--screenshots', args.screenshots, '--runner', args.runner],
                   cwd=ROOT, env=env, check=True)
    raise SystemExit(0)

entries = json.loads((args.output / 'index.json').read_text())
assert entries, 'Export examples first with make gallery-zib'
shots = args.output / args.screenshots
shots.mkdir(exist_ok=True)
player = ([args.runner] if args.runner else []) + [str(args.player.resolve())]
def platform_path(path):
    value = str(path.resolve())
    return 'Z:' + value.replace('/', '\\') if args.runner == 'wine' else value

for entry in entries:
    bundle = args.output / entry['bundle']
    subprocess.run([*player, '--check', platform_path(bundle)], env=env, check=True, timeout=60)
    shot = shots / (entry['id'].replace('/', '-') + '.png')
    capture = {**env, 'KRYON_CAPTURE_PATH': platform_path(shot)}
    subprocess.run([*player, platform_path(bundle)], cwd=shots, env=capture, check=True, timeout=60)
    pixels = png_pixels(shot)
    assert pixels[:2] == (960, 600), entry['id']
    assert any(sum(rgb(pixels, x, y)) < 500 for y in range(12, 280, 4)
               for x in range(24, 456, 4)), 'Empty example: ' + entry['id']
    print('Rendered ' + entry['id'] + ': ' + entry['name'], flush=True)
print(f'All {len(entries)} exported gallery ZIBs validated and rendered on a private display')
