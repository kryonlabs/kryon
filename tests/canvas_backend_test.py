#!/usr/bin/env python3
"""Compile Zi Canvas2D effects to Wasm and verify pixels in a private headless browser."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = Path(os.environ.get('ZIRAN_DIR', ROOT.parent / 'ziran'))
BIN = Path(os.environ.get('ZIRAN_BUILD_DIR', ZIRAN / 'build')) / 'bin'
EMCC = os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc'))
environment = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY'):
    environment.pop(name, None)
environment.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))

def run(args, **options):
    return subprocess.run(list(map(str,args)), env=environment, check=True, **options)

with tempfile.TemporaryDirectory(prefix='canvas-browser-', dir=ROOT / 'build') as path:
    work=Path(path)
    gen=work/'generated'
    compiler = [BIN/'zi2c']
    if os.environ.get('ZIRAN_RUNNER'):
        compiler = ['sh', os.environ['ZIRAN_RUNNER'], *compiler]
    run([*compiler,'--no-main','--define','PLATFORM_WEB','--root',ROOT/'tests',
         '--module-path',ROOT/'src/ui','--module-path',ROOT/'src/backend','--module-path',ZIRAN/'std',
         '-o',gen,ROOT/'tests/canvas_backend_behavior.zi'])
    shell=work/'shell.html'
    shell.write_text('''<!doctype html><html><head><meta charset="utf-8"><style>body{margin:0}canvas{width:160px;height:120px}</style></head><body><div id="canvas-frame" style="width:160px;height:120px"><canvas id="canvas" width="160" height="120"></canvas></div><pre id="result">pending</pre><script>var Module={onExit:function(code){document.getElementById('result').textContent=code===0?'PASS':('FAIL '+code+' '+globalThis.canvasTestError)},onAbort:function(reason){document.getElementById('result').textContent='ABORT '+reason}};</script>{{{ SCRIPT }}}</body></html>''')
    libs=['--js-library',ZIRAN/'web/ziran_web.js','-sEXPORTED_RUNTIME_METHODS=FS',ROOT/'src/backend/canvas_varargs.c']
    run([EMCC,'-O1','-I'+str(ZIRAN/'include'),'-iquote',gen,*sorted(gen.rglob('*.c')),
         *libs,'--embed-file',str(ROOT/'assets/fonts/LiberationSans-Regular.ttf')+'@/test-font.ttf','--js-library',ROOT/'tests/canvas_backend_effects.js','-sASYNCIFY','-sSINGLE_FILE=1',
         '-sEXIT_RUNTIME=1','-sENVIRONMENT=web','--shell-file',shell,'-o',work/'test.html'])
    run(['node',ROOT/'tests/canvas_backend_browser.mjs',(work/'test.html').as_uri(),work/'profile'],timeout=35)
print('Canvas2D Zi/Wasm browser pixels, nested clips, input and lifecycle passed')
