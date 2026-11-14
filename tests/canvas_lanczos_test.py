#!/usr/bin/env python3
"""Smooth Canvas images in private headless Firefox match Pillow's Lanczos.

The page draws through the Canvas raster provider and makes each copy in
slices between frames; Marionette reads the frames back for
tests/lanczos_goldens.py. No Node, owner browser or display.
"""
import base64
import subprocess
import sys
import time

from browser_behavior import ROOT, environment, EMCC, run
from toolchain import ZIRAN, ZIRAN_ROOT
import avif_firefox_test as harness

WORK = ROOT / 'build/scratch/canvas-lanczos'
FIXTURES = WORK / 'fixtures'
GOLDENS = ROOT / 'tests/lanczos_goldens.py'
CHUNK = 32768


def script(command, text):
    return command('WebDriver:ExecuteScript', {'script': text, 'args': []})['value']


def build():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-I', str(GOLDENS), 'fixtures', str(FIXTURES)], check=True)
    generated = WORK / 'generated'
    run([ZIRAN, 'build', '--project', '--root', ROOT / 'tests', '--define', 'PLATFORM_WEB',
         '--target=c', '--target-os', 'web', '--target-cpu', 'wasm32', '--no-main', '-o', generated, ROOT / 'tests/canvas_lanczos_test.zi'])
    shell = WORK / 'shell.html'
    shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body style="margin:0">'
                     '<div id="canvas-frame" style="width:128px;height:128px"><canvas id="canvas" width="128" '
                     'height="128" style="width:128px;height:128px"></canvas></div><pre id="result">pending</pre>'
                     '{{{ SCRIPT }}}</body></html>')
    run([EMCC, '-O2', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', generated, *sorted(generated.rglob('*.c')),
         '--js-library', ZIRAN_ROOT / 'web/ziran_web.js', '-sASYNCIFY', '-sALLOW_MEMORY_GROWTH=1',
         '-sSTACK_SIZE=1048576', '-sSINGLE_FILE=1', '-sENVIRONMENT=web',
         '--preload-file', f'{FIXTURES}@/lanczos', '--shell-file', shell, '-o', WORK / 'test.html'])


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    build()
    harness.WORK = WORK
    lines = []
    with harness.firefox(WORK) as (command, base):
        command('WebDriver:Navigate', {'url': f'{base}/test.html'})
        deadline = time.monotonic() + 120
        result = 'pending'
        while time.monotonic() < deadline:
            result = script(command, 'return document.getElementById("result")?.textContent')
            if result not in (None, 'pending'):
                break
            time.sleep(0.1)
        assert result == 'PASS', f'Canvas Lanczos page: {result}'
        count = script(command, 'return globalThis.lanczosFrames.length')
        for frame in range(count):
            line = script(command, f'return globalThis.lanczosFrames[{frame}].line')
            data = b''
            while len(data) < 65536:
                data += base64.b64decode(script(command,
                    f'const d = globalThis.lanczosFrames[{frame}].data.subarray({len(data)}, {len(data) + CHUNK});'
                    'let s = ""; for (const b of d) s += String.fromCharCode(b); return btoa(s);'))
            _, kind, index = line.split()[:3]
            (FIXTURES / f'{kind}-{index}.rgba').write_bytes(data)
            lines.append(line)
    output = WORK / 'run.out'
    output.write_text('\n'.join(lines) + '\n')
    print(output.read_text(), end='')
    subprocess.run([sys.executable, '-I', str(GOLDENS), 'compare', str(FIXTURES), str(output)], check=True)
    print('Canvas draws settled Lanczos copies 1:1 in Firefox, made a slice per frame')


if __name__ == '__main__':
    main()
