#!/usr/bin/env python3
"""Run Zi PCM transforms and the real WebAudio effects through Wasm."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = Path(os.environ.get('ZIRAN_DIR', ROOT.parent / 'ziran'))
BIN = Path(os.environ.get('ZIRAN_BUILD_DIR', ZIRAN / 'build')) / 'bin'
RUNNER = shlex.split(os.environ.get('ZIRAN_RUNNER', ''))
ENV = dict(os.environ)
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY'):
    ENV.pop(key, None)
ENV.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))
def run(command):
    subprocess.run(list(map(str, command)), env=ENV, check=True)
with tempfile.TemporaryDirectory(prefix='canvas-audio-', dir=ROOT / 'build') as directory:
    work = Path(directory)
    generated = work / 'generated'
    run([*RUNNER, BIN / 'zi2c', '--no-main',
         '--define', 'PLATFORM_WEB', '--root', ROOT, '--module-path', ROOT / 'src/ui',
         '--module-path', ROOT / 'src/backend', '--module-path', ZIRAN / 'std',
         '-o', generated, ROOT / 'tests/canvas_audio_behavior.zi'])
    includes = ['-I' + str(ZIRAN / 'include')]
    includes += ['-iquote' + str(p) for p in sorted({p.parent for p in generated.rglob('*.h')})]
    run([os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc')), '-O1',
         *includes, *sorted(generated.rglob('*.c')), '--js-library', ZIRAN / 'web/ziran_web.js',
         '--js-library', ROOT / 'tests/canvas_audio_effects.js',
         '--pre-js', ROOT / 'tests/canvas_audio_fixture.js', '-sASYNCIFY', '-sEXIT_RUNTIME=1', '-sEXPORTED_RUNTIME_METHODS=FS',
         '-sENVIRONMENT=node', '-sWASM_ASYNC_COMPILATION=0', '-o', work / 'test.js'])
    run(['node', work / 'test.js'])
print('Canvas audio PCM, export, sound/music, callback and resource lifetime Wasm test passed')
