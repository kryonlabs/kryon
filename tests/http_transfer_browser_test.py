#!/usr/bin/env python3
"""Run http_transfer's fetch transport in private headless Chromium against a local server."""
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


with tempfile.TemporaryDirectory(prefix='http-transfer-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    generated = work / 'generated'
    run([BIN / 'zi2c', '--no-main', '--define', 'PLATFORM_WEB', '--root', ROOT / 'tests',
         '--module-path', ROOT / 'src/backend', '--module-path', ZIRAN / 'std',
         '-o', generated, ROOT / 'tests/http_transfer_browser_behavior.zi'])
    shell = work / 'shell.html'
    shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body>'
                     '<pre id="result">pending</pre>{{{ SCRIPT }}}</body></html>')
    run([EMCC, '-O1', '-I' + str(ZIRAN / 'include'), '-iquote', generated, *sorted(generated.rglob('*.c')),
         '--js-library', ZIRAN / 'web/ziran_web.js', '-sASYNCIFY', '-sSINGLE_FILE=1',
         '-sENVIRONMENT=web', '--shell-file', shell, '-o', work / 'test.html'])
    server = subprocess.Popen(['python3', ROOT / 'tests/http_transfer_server.py'], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, text=True)
    try:
        port = server.stdout.readline().strip()
        run(['node', ROOT / 'tests/canvas_backend_browser.mjs',
             (work / 'test.html').as_uri() + '?port=' + port, work / 'profile'], timeout=45)
    finally:
        server.kill()
        server.wait()
print('HTTP transfer in the browser: GET, POST with headers, binary answers with their length, oversized and slow answers, and cancelling passed')
