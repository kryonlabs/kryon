#!/usr/bin/env python3
"""Compile file_export for the browser and check its downloads in private headless Chromium."""
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import BIN, ZIRAN_ROOT as ZIRAN

ROOT = Path(__file__).resolve().parents[1]
EMCC = os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc'))
environment = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY'):
    environment.pop(name, None)
environment.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))


def run(args, **options):
    return subprocess.run(list(map(str, args)), env=environment, check=True, **options)


with tempfile.TemporaryDirectory(prefix='file-export-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    generated = work / 'generated'
    run([BIN / 'zi2c', '--no-main', '--define', 'PLATFORM_WEB', '--root', ROOT / 'tests',
         '--module-path', ROOT / 'src/backend', '--module-path', ZIRAN / 'std',
         '-o', generated, ROOT / 'tests/file_export_browser_behavior.zi'])
    shell = work / 'shell.html'
    shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body>'
                     '<pre id="result">pending</pre>{{{ SCRIPT }}}</body></html>')
    run([EMCC, '-O1', '-I' + str(ZIRAN / 'include'), '-iquote', generated, *sorted(generated.rglob('*.c')),
         '--js-library', ZIRAN / 'web/ziran_web.js', '-sASYNCIFY', '-sSINGLE_FILE=1',
         '-sENVIRONMENT=web', '--shell-file', shell, '-o', work / 'test.html'])
    (work / 'downloads').mkdir()
    run(['node', ROOT / 'tests/file_export_browser.mjs', (work / 'test.html').as_uri(), work / 'profile', work / 'downloads'],
        timeout=45)
print('File export in the browser: bytes and files downloaded with their names, refusals and cleanup passed')
