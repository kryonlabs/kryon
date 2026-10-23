#!/usr/bin/env python3
"""Build a preserved Canvas2D benchmark artifact; compare with its paired browser runner."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from toolchain import BIN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
if (output / 'index.html').exists():
    parser.error('refusing to replace a preserved benchmark artifact')
environment = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'GDK_DISPLAY'):
    environment.pop(name, None)
environment.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))
generated = output / 'c'
subprocess.run([str(BIN / 'zi2c'), '--no-main', '--root', str(ROOT / 'tests'),
                '--module-path', str(ROOT / 'src/backend'), '--module-path', str(ROOT / 'src/ui'),
                '--module-path', str(ZIRAN_ROOT / 'std'),
                '-o', str(generated), str(ROOT / 'tests/canvas_performance.zi')],
               env=environment, check=True)
shell = output / 'shell.html'
shell.write_text('''<!doctype html><meta charset="utf-8"><style>body{margin:0}</style>
<div id="canvas-frame" style="width:512px;height:384px"><canvas id="canvas"></canvas></div>
<script>var Module={onRuntimeInitialized(){globalThis.ready=true}};</script>{{{ SCRIPT }}}''')
emcc = os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc'))
command = [emcc, '-O2', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', str(generated),
           *map(str, sorted(generated.rglob('*.c'))), '--js-library', str(ZIRAN_ROOT / 'web/ziran_web.js'),
           '-sEXPORTED_FUNCTIONS=_Setup,_Draw', '-sEXPORTED_RUNTIME_METHODS=ccall',
           '-sALLOW_MEMORY_GROWTH=1', '-sENVIRONMENT=web', '--shell-file', str(shell),
           '-o', str(output / 'index.html')]
subprocess.run(command, env=environment, check=True)
sources = [ROOT / 'tests/canvas_performance.zi', ROOT / 'src/backend/page_texture.zi',
           ZIRAN_ROOT / 'web/ziran_web.js', BIN / 'zi2c']
metadata = {'command': command, 'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
(output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(output / 'index.html')
