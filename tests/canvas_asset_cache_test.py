#!/usr/bin/env python3
"""The Canvas host keeps the images a scene draws in private headless Firefox.

A page drawing more images than a small cache held decoded each of them
again every frame; tests/canvas_asset_cache_test.zi checks that forty
pictures keep their textures across frames and that a full cache replaces
the picture used longest ago. No Node, owner browser or display.
"""
import struct
import time
import zlib

from browser_behavior import ROOT, EMCC, run
from toolchain import ZIRAN, ZIRAN_ROOT
import avif_firefox_test as harness

WORK = ROOT / 'build/scratch/canvas-asset-cache'
FIXTURES = WORK / 'fixtures'
PICTURES = 140


def png(index):
    """A 4x4 picture of one colour of its own."""
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    pixel = bytes((index * 37 % 256, index * 91 % 256, index % 256))
    rows = b''.join(b'\0' + pixel * 4 for _ in range(4))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def build():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    for index in range(PICTURES):
        (FIXTURES / f'{index}.png').write_bytes(png(index))
    generated = WORK / 'generated'
    run([ZIRAN, 'build', '--project', '--root', ROOT / 'tests', '--define', 'PLATFORM_WEB',
         '--target=c', '--target-os', 'web', '--target-cpu', 'wasm32', '--no-main', '-o', generated, ROOT / 'tests/canvas_asset_cache_test.zi'])
    shell = WORK / 'shell.html'
    shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body style="margin:0">'
                     '<div id="canvas-frame" style="width:64px;height:64px"><canvas id="canvas" width="64" '
                     'height="64" style="width:64px;height:64px"></canvas></div><pre id="result">pending</pre>'
                     '{{{ SCRIPT }}}</body></html>')
    run([EMCC, '-O1', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', generated, *sorted(generated.rglob('*.c')),
         '--js-library', ZIRAN_ROOT / 'web/ziran_web.js', '-sASYNCIFY', '-sALLOW_MEMORY_GROWTH=1',
         '-sSTACK_SIZE=1048576', '-sSINGLE_FILE=1', '-sENVIRONMENT=web',
         '--preload-file', f'{FIXTURES}@/cache', '--shell-file', shell, '-o', WORK / 'test.html'])


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    build()
    harness.WORK = WORK
    with harness.firefox(WORK) as (command, base):
        command('WebDriver:Navigate', {'url': f'{base}/test.html'})
        deadline = time.monotonic() + 120
        result = 'pending'
        while time.monotonic() < deadline:
            result = command('WebDriver:ExecuteScript',
                             {'script': 'return document.getElementById("result")?.textContent', 'args': []})['value']
            if result not in (None, 'pending'):
                break
            time.sleep(0.1)
    assert result == 'PASS', f'Canvas image cache page: {result}'
    print('Canvas keeps forty images a frame decoded once and replaces the one used longest ago')


if __name__ == '__main__':
    main()
