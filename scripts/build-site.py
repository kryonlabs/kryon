#!/usr/bin/env python3
"""Rebuild the site's existing Canvas demo/editor and the shared widget gallery."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    env.pop(name, None)
emcc = env.get('EMCC') or shutil.which('emcc') or str(Path.home() / 'emsdk/upstream/emscripten/emcc')
if not Path(emcc).is_file():
    raise SystemExit('Emscripten is required. Activate emsdk or set EMCC.')
env['EMCC'] = emcc
for command, directory in [
    (['python3', 'scripts/site-metadata.py'], ROOT),
    (['sh', 'build.sh'], ROOT / 'docs/site/demo'),
    (['python3', 'build_preview.py', '--emcc', emcc], ROOT / 'docs/site/demo'),
    (['python3', 'scripts/build-gallery.py'], ROOT),
]:
    subprocess.run(command, cwd=directory, env=env, check=True)
print('Built the full Kryon website: demo, browser compiler, preview host and widget gallery')
