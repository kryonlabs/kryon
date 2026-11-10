#!/usr/bin/env python3
"""Prove the production Canvas2D web app works with WebGL requests rejected."""

from __future__ import annotations

import json
import base64
import os
from pathlib import Path
import re
import select
import shutil
import struct
import subprocess
import time
import zlib
from urllib.parse import urlsplit

from toolchain import ZIRAN, ZIRAN_ROOT


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/cnw'
PROJECT = WORK / 'project'
OUTPUT = PROJECT / 'build/canvas_no_webgl-web.html'
ENV = {key: value for key, value in os.environ.items() if key not in (
    'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY', 'GDK_DISPLAY',
    'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'XDG_SESSION_ID',
    'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH', 'KRYON_TEST_DRIVER')}
ENV.update(
    YUE_DESKTOP_RECOVERY='0',
    EMCC_CORES='1',
    EM_CACHE=str(ROOT / 'build/emscripten-cache'),
    TMPDIR=str(WORK / 'tmp'),
)


def run(command, directory, timeout=900):
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV,
                   check=True, timeout=timeout)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (struct.pack('>I', len(data)) + kind + data +
            struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))


def write_color_probe(path: Path, opaque: bytes, transparent: bytes):
    rows = []
    for _ in range(64):
        rows.append(opaque * 32 + transparent * 32)
    raw = b''.join(b'\x00' + row for row in rows)
    path.write_bytes(
        b'\x89PNG\r\n\x1a\n' +
        png_chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 64, 8, 6, 0, 0, 0)) +
        png_chunk(b'IDAT', zlib.compress(raw, 9)) +
        png_chunk(b'IEND', b''))


def prepare_project():
    shutil.rmtree(WORK, ignore_errors=True)
    (PROJECT / 'src').mkdir(parents=True)
    (PROJECT / 'assets').mkdir()
    (WORK / 'tmp').mkdir()
    write_color_probe(PROJECT / 'assets/alpha.png',
                      b'\xdc\x1e\x3c\xff', b'\x00\x00\x00\x00')
    (PROJECT / 'assets/nested').mkdir()
    write_color_probe(PROJECT / 'assets/nested/duplicate.png',
                      b'\x1e\x3c\xdc\xff', b'\x00\x00\x00\x00')
    (PROJECT / 'ziran.toml').write_text(
        '[package]\nname = "canvas_no_webgl"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "canvas"\nhost = "src/host.zi"\n'
        'assets = "assets"\n')
    (PROJECT / 'src/host.zi').write_text(
        'using App :: #import "app";\n'
        '#import "kryon/canvas_app_run"\n\n'
        '#program_export\n'
        'CanvasAppMain :: () -> s64 { return App.Main() }\n')
    (PROJECT / 'ziran.local.toml').write_text(
        '[overrides]\nkryon = "%s"\nziran = "%s"\n' % (ROOT, ZIRAN_ROOT))
    shutil.copyfile(ROOT / 'tests/canvas_no_webgl_app.zi',
                    PROJECT / 'src/app.zi')
    run([ZIRAN, 'lock'], PROJECT)
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', 'web'], PROJECT)
    if not OUTPUT.is_file() or OUTPUT.stat().st_size < 100_000:
        raise AssertionError('production Canvas2D HTML was not emitted')
    page = OUTPUT.read_bytes()
    assert page.count(b'class="kryon-file" data-mount="/assets/alpha.png"') == 1
    assert page.count(b'class="kryon-file" data-mount="/assets/nested/duplicate.png"') == 1


def verify_embed_packaging():
    page = OUTPUT.read_bytes()

    def production_module(name):
        marker = f'<script type="text/plain" id="kryon-module-{name}">\n'.encode()
        start = page.index(marker) + len(marker)
        end = page.index(b'\n</script>', start)
        return page[start:end]

    directory = WORK / 'embed-check'
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir()
    (directory / 'simd128.js').write_bytes(production_module('simd128'))
    (directory / 'baseline.js').write_bytes(production_module('baseline'))
    output = directory / 'page.html'
    with output.open('wb') as stream:
        subprocess.run(
            ['sh', ROOT / 'mk/web-page.sh',
             PROJECT / 'build/generated/web/page-shell.html',
             directory / 'simd128.js', directory / 'baseline.js',
             PROJECT / 'assets/alpha.png@/root.png',
             PROJECT / 'assets/nested@/deep/nested',
             PROJECT / 'assets/nested/duplicate.png@/root.png'],
            cwd=PROJECT, env=ENV, check=True, stdout=stream)
    embedded = re.findall(
        rb'class="kryon-file" data-mount="([^"]+)">([A-Za-z0-9+/=]+)',
        output.read_bytes())
    mounts = [mount.decode() for mount, _ in embedded]
    assert mounts == ['/root.png', '/deep/nested/duplicate.png'], mounts
    payloads = {mount.decode(): base64.b64decode(data) for mount, data in embedded}
    assert payloads['/root.png'] == (PROJECT / 'assets/alpha.png').read_bytes()
    assert payloads['/deep/nested/duplicate.png'] == (
        PROJECT / 'assets/nested/duplicate.png').read_bytes()
    print('production embed mounts: PASS (explicit file, nested directory, first duplicate wins)')


class DevToolsError(RuntimeError):
    def __init__(self, method, error):
        super().__init__(f'{method}: {error}')
        self.method = method


class Browser:
    def __init__(self):
        profile = WORK / 'p'
        shutil.rmtree(profile, ignore_errors=True)
        profile.mkdir()
        self.log = (WORK / 'chromium.log').open('wb')
        self.process = subprocess.Popen(
            ['sh', '-c', 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', 'chromium',
             os.environ.get('CHROMIUM', 'chromium'), '--headless', '--no-sandbox',
             '--disable-gpu', '--disable-webgl', '--disable-software-rasterizer',
             '--disable-gpu-compositing', '--use-gl=disabled',
             '--disable-dev-shm-usage', '--no-first-run',
             '--no-default-browser-check', '--remote-debugging-pipe',
             '--user-data-dir=' + str(profile), 'about:blank'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=ENV, start_new_session=True)
        self.received = b''
        self.sequence = 0
        self.session = None
        self.errors = []
        target = next(item for item in self.command('Target.getTargets')['targetInfos']
                      if item['type'] == 'page')
        self.session = self.command('Target.attachToTarget', targetId=target['targetId'],
                                    flatten=True)['sessionId']
        for domain in ('Runtime', 'Page', 'Network'):
            self.command(domain + '.enable')
        self.command('Page.addScriptToEvaluateOnNewDocument', source=r'''
            globalThis.canvasStats = {
              rasterCalls: 0, rasterCpu: 0, presentCalls: 0, presentCpu: 0,
              webglRequests: 0, inputMark: 0, lastPresent: 0, lastVisible: 0,
              latencyMs: 0, composition: [], profiles: {}, setupCalls: 0,
              setupCpu: 0, frameRasterCalls: 0, frameRasterCpu: 0,
              maxFrameRasterCalls: 0, maxFrameRasterCpu: 0
            };
            for (const type of ['compositionstart', 'compositionupdate', 'compositionend', 'compositioncancel'])
              addEventListener(type, event => canvasStats.composition.push(
                {type, data: event.data, trusted: event.isTrusted}), true);
            for (const type of ['pointerdown', 'touchstart', 'keydown'])
              addEventListener(type, () => {
                if (!canvasStats.inputMark) canvasStats.inputMark = performance.now();
              }, true);
            for (const prototype of [HTMLCanvasElement.prototype, OffscreenCanvas.prototype]) {
              const original = prototype.getContext;
              prototype.getContext = function(kind, ...args) {
                if (kind === 'webgl' || kind === 'webgl2' || kind === 'experimental-webgl') {
                  canvasStats.webglRequests++;
                  return null;
                }
                return original.call(this, kind, ...args);
              };
            }
            const methods = ['clearRect', 'fillRect', 'strokeRect', 'drawImage',
              'fillText', 'strokeText', 'fill', 'stroke', 'putImageData',
              'beginPath', 'clip', 'save', 'restore',
              'setTransform', 'translate', 'rotate', 'scale'];
            const setupMethods = new Set(['beginPath', 'clip', 'save', 'restore',
              'setTransform', 'translate', 'rotate', 'scale']);
            function describe(context) {
              const canvas = context.canvas;
              if (!canvas) return 'detached';
              if (canvas.id === 'canvas') return 'front';
              const label = canvas.id || (canvas.dataset && canvas.dataset.kryonName) ||
                canvas.tagName || (canvas.constructor && canvas.constructor.name) || 'unknown';
              return label + ':' + canvas.width + 'x' + canvas.height;
            }
            for (const kind of ['CanvasRenderingContext2D', 'OffscreenCanvasRenderingContext2D']) {
              const prototype = globalThis[kind] && globalThis[kind].prototype;
              if (!prototype) continue;
              for (const name of methods) {
                const original = prototype[name];
                prototype[name] = function(...args) {
                  const started = performance.now();
                  const source = kind + ':' + describe(this);
                  const key = source + '.' + name;
                  const profile = canvasStats.profiles[key] ||
                    (canvasStats.profiles[key] = {count: 0, cpu: 0});
                  profile.count++;
                  const setup = setupMethods.has(name);
                  try { return original.apply(this, args); }
                  finally {
                    const finished = performance.now();
                    const elapsed = finished - started;
                    profile.cpu += elapsed;
                    if (setup) {
                      canvasStats.setupCalls++;
                      canvasStats.setupCpu += elapsed;
                    }
                    if (this.canvas && this.canvas.id === 'canvas') {
                      canvasStats.presentCalls++;
                      canvasStats.presentCpu += elapsed;
                      canvasStats.lastPresent = finished;
                      if (name === 'drawImage') {
                        canvasStats.maxFrameRasterCalls = Math.max(
                          canvasStats.maxFrameRasterCalls, canvasStats.frameRasterCalls);
                        canvasStats.maxFrameRasterCpu = Math.max(
                          canvasStats.maxFrameRasterCpu, canvasStats.frameRasterCpu);
                        canvasStats.frameRasterCalls = 0;
                        canvasStats.frameRasterCpu = 0;
                      }
                      requestAnimationFrame(() => requestAnimationFrame(() => {
                        canvasStats.lastVisible = performance.now();
                      }));
                    } else {
                      if (!setup) {
                        canvasStats.rasterCalls++;
                        canvasStats.rasterCpu += elapsed;
                        canvasStats.frameRasterCalls++;
                        canvasStats.frameRasterCpu += elapsed;
                      }
                    }
                  }
                };
              }
            }
        ''')

    def command(self, method, timeout=45, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session:
            message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b'\x00' in self.received:
                raw, self.received = self.received.split(b'\x00', 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                event = reply.get('method')
                if event == 'Runtime.exceptionThrown':
                    details = reply['params']['exceptionDetails']
                    self.errors.append((details.get('exception') or {}).get('description')
                                       or details.get('text'))
                if reply.get('id') == self.sequence:
                    if 'error' in reply:
                        raise DevToolsError(method, reply['error'])
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .1)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data:
                    raise RuntimeError('Chromium exited; see ' + str(WORK / 'chromium.log'))
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression):
        result = self.command('Runtime.evaluate', expression=expression,
                              returnByValue=True, awaitPromise=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:2000])
        return result.get('result', {}).get('value')

    def until(self, expression, what, seconds=20):
        deadline = time.monotonic() + seconds
        value = None
        while time.monotonic() < deadline:
            value = self.evaluate(expression)
            if value:
                return value
            time.sleep(.08)
        raise AssertionError(f'{what}: timed out at {value!r}')

    def open(self, url):
        self.command('Page.navigate', url=url)
        self.until("document.readyState === 'complete'", 'page load')

    def viewport(self, width, height, scale=1, mobile=False):
        self.command('Emulation.setDeviceMetricsOverride', width=width, height=height,
                     deviceScaleFactor=scale, mobile=mobile)

    def color_scheme(self, dark):
        self.command('Emulation.setEmulatedMedia', features=[
            {'name': 'prefers-color-scheme',
             'value': 'dark' if dark else 'light'}])

    def frames(self, count=3):
        self.evaluate(f'''(async () => {{
          for (let index = 0; index < {count}; index++) {{
            await new Promise(resolve => requestAnimationFrame(resolve));
          }}
          return true;
        }})()''')

    def sample(self, x, y):
        return self.evaluate(f'''(() => {{
          const canvas = document.getElementById('canvas');
          const scale = canvas.width / canvas.clientWidth;
          const pixel = canvas.getContext('2d').getImageData(
            Math.round({x} * scale), Math.round({y} * scale), 1, 1).data;
          return Array.from(pixel);
        }})()''')

    def region(self, x, y, width, height):
        return self.evaluate(f'''(() => {{
          const canvas = document.getElementById('canvas');
          const scale = canvas.width / canvas.clientWidth;
          const x = Math.round({x} * scale), y = Math.round({y} * scale);
          const width = Math.round({width} * scale), height = Math.round({height} * scale);
          const pixels = canvas.getContext('2d').getImageData(x, y, width, height).data;
          let sum = 0, nonbackground = 0;
          for (let index = 0; index < pixels.length; index += 4) {{
            sum = (sum + pixels[index] * 65536 + pixels[index + 1] * 256 +
              pixels[index + 2] + pixels[index + 3] * 16777216) % 2147483647;
            if (pixels[index + 3] !== 0) nonbackground++;
          }}
          return {{sum, nonbackground, bytes: Array.from(pixels.slice(0, 4))}};
        }})()''')

    def wait_region_change(self, x, y, width, height, before, what):
        return self.until(f'''(() => {{
          const canvas = document.getElementById('canvas');
          const scale = canvas.width / canvas.clientWidth;
          const x = Math.round({x} * scale), y = Math.round({y} * scale);
          const width = Math.round({width} * scale), height = Math.round({height} * scale);
          const pixels = canvas.getContext('2d').getImageData(x, y, width, height).data;
          let sum = 0, nonbackground = 0;
          for (let index = 0; index < pixels.length; index += 4) {{
            sum = (sum + pixels[index] * 65536 + pixels[index + 1] * 256 +
              pixels[index + 2] + pixels[index + 3] * 16777216) % 2147483647;
            if (pixels[index + 3] !== 0) nonbackground++;
          }}
          return sum !== {before['sum']} ? {{sum, nonbackground}} : null;
        }})()''', what)

    def mark_input(self):
        self.evaluate('canvasStats.inputMark = 0; canvasStats.latencyMs = 0')

    def mouse(self, x, y):
        for kind in ('mousePressed', 'mouseReleased'):
            self.command('Input.dispatchMouseEvent', type=kind, x=x, y=y,
                         button='left', clickCount=1)

    def touch(self, x, y):
        self.command('Input.dispatchTouchEvent', type='touchStart',
                     touchPoints=[{'x': x, 'y': y, 'id': 1, 'radiusX': 2,
                                   'radiusY': 2, 'force': 1}])
        self.command('Input.dispatchTouchEvent', type='touchEnd', touchPoints=[])

    def key(self, key, code, virtual, text='', modifiers=0, down=True):
        self.command('Input.dispatchKeyEvent', type='keyDown' if down else 'keyUp',
                     key=key, code=code, windowsVirtualKeyCode=virtual,
                     nativeVirtualKeyCode=virtual, text=text, modifiers=modifiers)

    def tap_key(self, key, code, virtual, text='', modifiers=0):
        self.key(key, code, virtual, text, modifiers, True)
        self.key(key, code, virtual, '', modifiers, False)

    def composition(self, text, committed):
        self.command('Input.imeSetComposition', text=text, selectionStart=0,
                     selectionEnd=len(text), replacementStart=0,
                     replacementEnd=len(text))
        self.frames(2)
        if committed:
            self.command('Input.insertText', text=committed)
            self.frames(3)

    def stats(self):
        value = self.evaluate('''(() => {
          const navigation = performance.getEntriesByType('navigation')[0] || {};
          const resources = performance.getEntriesByType('resource');
          return {...canvasStats, navigationBytes: navigation.transferSize || 0,
            resourceBytes: resources.reduce((sum, item) => sum + item.transferSize, 0),
            resourceCount: resources.length};
        })()''')
        if self.errors:
            raise AssertionError('page exception: ' + '; '.join(self.errors[:3]))
        return value

    def reset_stats(self):
        self.evaluate('''canvasStats.rasterCalls = 0; canvasStats.rasterCpu = 0;
          canvasStats.presentCalls = 0; canvasStats.presentCpu = 0;
          canvasStats.setupCalls = 0; canvasStats.setupCpu = 0;
          canvasStats.frameRasterCalls = 0; canvasStats.frameRasterCpu = 0;
          canvasStats.maxFrameRasterCalls = 0; canvasStats.maxFrameRasterCpu = 0;
          canvasStats.profiles = {};''')

    def close(self):
        try:
            self.command('Browser.close', timeout=3)
        except (BrokenPipeError, RuntimeError, TimeoutError):
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(self.process.pid, 15)
            self.process.wait(timeout=5)
        self.log.close()


def close_color(actual, expected, tolerance=12):
    return all(abs(actual[index] - expected[index]) <= tolerance for index in range(4))


def luma(color):
    return .2126 * color[0] + .7152 * color[1] + .0722 * color[2]


def transfer_sizes():
    files = sorted(path for path in OUTPUT.parent.rglob('*') if path.is_file())
    return {str(path.relative_to(OUTPUT.parent)): path.stat().st_size for path in files}


def exercise(url):
    browser = Browser()
    try:
        browser.viewport(640, 720, 1, mobile=True)
        browser.color_scheme(False)
        browser.open(url)
        browser.until('''(() => {
          const canvas = document.getElementById('canvas');
          return canvas && canvas.clientWidth === 640 && canvas.clientHeight === 720 &&
            canvas.width === 640;
        })()''', 'Canvas2D app opened')
        browser.until("canvasStats.webglRequests >= 1", 'optional WebGL path was probed')
        browser.until('''(() => {
          const left = [document.getElementById('canvas').getContext('2d').getImageData(28, 250, 1, 1).data,
            document.getElementById('canvas').getContext('2d').getImageData(112, 250, 1, 1).data];
          return left.every(color => color[0] > 150 && color[1] < 100 && color[3] === 255);
        })()''', 'normal and fallback mesh images decoded')
        assert close_color(browser.sample(200, 250), [30, 60, 220, 255]), browser.sample(200, 250)

        light = browser.sample(620, 20)
        assert luma(light) > 180, light
        default_button_light = browser.region(20, 20, 124, 32)
        assert default_button_light['nonbackground'] > 100, default_button_light
        assert close_color(browser.sample(220, 20), [91, 42, 134, 255]), browser.sample(220, 20)
        assert browser.region(16, 174, 300, 32)['nonbackground'] > 30, 'Unicode text did not paint'
        assert close_color(browser.sample(30, 250), [220, 30, 60, 255], 20), browser.sample(30, 250)
        assert browser.sample(80, 260) == light, (browser.sample(80, 260), light)
        assert close_color(browser.sample(112, 250), [220, 30, 60, 255], 20), browser.sample(112, 250)
        assert browser.sample(152, 260) == light, (browser.sample(152, 260), light)
        assert browser.sample(150, 316) == light, (browser.sample(150, 316), light)

        browser.color_scheme(True)
        dark = browser.until('''(() => {
          const color = Array.from(document.getElementById('canvas').getContext('2d').getImageData(620, 20, 1, 1).data);
          return (.2126 * color[0] + .7152 * color[1] + .0722 * color[2]) < 80 ? color : null;
        })()''', 'dark theme became visible')
        assert dark != light and luma(dark) < 80 < luma(light), (light, dark)
        assert close_color(browser.sample(220, 20), [91, 42, 134, 255]), browser.sample(220, 20)
        assert browser.region(20, 20, 124, 32)['sum'] != default_button_light['sum']

        button_before = browser.region(20, 24, 124, 24)
        browser.mark_input()
        browser.mouse(50, 36)
        browser.wait_region_change(20, 24, 124, 24, button_before, 'pointer click changed button')
        browser.frames(2)
        warm = browser.stats()
        browser.evaluate('''if (canvasStats.lastVisible >= canvasStats.inputMark) {
          canvasStats.latencyMs = canvasStats.lastVisible - canvasStats.inputMark;
        }''')
        warm = browser.stats()
        assert warm['rasterCalls'] > 0 and warm['latencyMs'] > 0, warm

        browser.mouse(50, 88)
        browser.frames(2)
        field_before = browser.region(20, 76, 250, 24)
        browser.tap_key('Control', 'ControlLeft', 17, modifiers=2)
        browser.tap_key('a', 'KeyA', 65, modifiers=2)
        browser.tap_key('Backspace', 'Backspace', 8)
        browser.tap_key('Z', 'KeyZ', 90, 'Z')
        browser.tap_key('i', 'KeyI', 73, 'i')
        browser.tap_key('r', 'KeyR', 82, 'r')
        browser.tap_key('a', 'KeyA', 65, 'a')
        browser.tap_key('n', 'KeyN', 78, 'n')
        browser.wait_region_change(20, 76, 250, 24, field_before, 'keyboard text edited field')
        assert browser.region(20, 76, 250, 24)['nonbackground'] > 10

        composition_before = browser.region(20, 76, 250, 24)
        browser.composition('日本', '日本語')
        browser.wait_region_change(20, 76, 250, 24, composition_before, 'IME text committed')
        events = browser.evaluate('canvasStats.composition')
        assert [item['type'] for item in events[:2]] == [
            'compositionstart', 'compositionupdate'], events
        assert events[-1]['type'] == 'compositionend', events
        assert all(item['trusted'] for item in events[:2]), events
        assert any(item['data'] == '日本語' for item in events), events

        marker_before = browser.region(104, 220, 18, 18)
        browser.tap_key('Enter', 'Enter', 13)
        browser.wait_region_change(104, 220, 18, 18, marker_before, 'Enter committed focused field')

        checkbox_before = browser.region(18, 128, 24, 24)
        browser.touch(40, 142)
        browser.wait_region_change(18, 128, 24, 24, checkbox_before, 'touch changed checkbox')

        browser.tap_key('Tab', 'Tab', 9)
        browser.frames(6)
        time.sleep(.25)
        browser.reset_stats()
        browser.frames(6)
        time.sleep(.4)
        idle = browser.stats()
        assert idle['rasterCalls'] == 0, idle
        assert idle['presentCalls'] > 0, idle

        browser.reset_stats()
        browser.mark_input()
        button_second = browser.region(20, 24, 124, 24)
        stable_second = browser.region(16, 68, 280, 42)
        browser.mouse(50, 36)
        browser.wait_region_change(20, 24, 124, 24, button_second, 'second pointer click')
        assert browser.region(16, 68, 280, 42) == stable_second, 'retained pixels changed outside damage'
        browser.frames(2)
        partial = browser.stats()
        browser.evaluate('''if (canvasStats.lastVisible >= canvasStats.inputMark) {
          canvasStats.latencyMs = canvasStats.lastVisible - canvasStats.inputMark;
        }''')
        partial = browser.stats()
        assert partial['rasterCalls'] > 0 and partial['latencyMs'] > 0, partial

        browser.reset_stats()
        browser.viewport(390, 744, 1, mobile=True)
        browser.until("document.getElementById('canvas').width === 390", 'resize full repaint')
        browser.frames(4)
        full = browser.stats()
        assert full['maxFrameRasterCalls'] > 0, full

        for width in (320, 390, 768, 1024, 1366):
            browser.viewport(width, 720, 1, mobile=True)
            browser.until(f"document.getElementById('canvas').clientWidth === {width}",
                          f'{width}px responsive layout')
            browser.frames(2)
            layout = browser.evaluate('''(() => ({
              scrollWidth: document.documentElement.scrollWidth,
              clientWidth: document.documentElement.clientWidth,
              button: Array.from(document.getElementById('canvas').getContext('2d').getImageData(220, 20, 1, 1).data).slice(0, 4)
            }))()''')
            assert layout['scrollWidth'] <= layout['clientWidth'] + 1, (width, layout)
            assert close_color(list(layout['button']), [91, 42, 134, 255]), (width, layout)

        browser.reset_stats()
        browser.viewport(320, 640, 2, True)
        browser.until('''document.getElementById('canvas').width === 640 &&
          document.getElementById('canvas').clientWidth === 320''', 'device scale change')
        browser.frames(4)
        scaled = browser.stats()
        assert scaled['rasterCalls'] > 0, scaled
        assert close_color(browser.sample(220, 20), [91, 42, 134, 255]), browser.sample(220, 20)
        assert browser.sample(152, 260) == browser.sample(300, 20), (
            browser.sample(152, 260), browser.sample(300, 20))

        final = browser.stats()
        sizes = transfer_sizes()
        assert final['webglRequests'] >= 1, final
        assert final['composition'], final
        assert not browser.errors, browser.errors

        print('Canvas2D no-WebGL production app: PASS')
        print(f"WebGL requests rejected: {final['webglRequests']}; software mesh pixels verified")
        print(f"light background={light}; dark background={dark}; typed style forwarded")
        print(f"warm damaged frame: {partial['maxFrameRasterCalls']} Canvas raster calls, {partial['maxFrameRasterCpu']:.3f} ms raster CPU, event-to-visible {partial['latencyMs']:.3f} ms")
        print(f"full repaint after resize: {full['maxFrameRasterCalls']} Canvas raster calls, {full['maxFrameRasterCpu']:.3f} ms raster CPU")
        print(f"DPR 2 repaint: {scaled['rasterCalls']} Canvas raster calls, {scaled['rasterCpu']:.3f} ms raster CPU")
        print(f"idle retained frame: {idle['rasterCalls']} Canvas raster calls in 400 ms; {idle['setupCalls']} setup calls, {idle['setupCpu']:.3f} ms; {idle['presentCalls']} present calls, {idle['presentCpu']:.3f} ms")
        print(f"download transfer: navigation={final['navigationBytes']} B resources={final['resourceBytes']} B files={sum(sizes.values())} B in {len(sizes)} files")
        for name, size in sizes.items():
            print(f'  {size:9d} B  {name}')
    finally:
        browser.close()


def main():
    prepare_project()
    verify_embed_packaging()
    from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            name = urlsplit(self.path).path.lstrip('/')
            if not name or any(part in ('', '.', '..') for part in name.split('/')):
                self.send_error(404)
                return
            path = OUTPUT.parent / name
            if not path.is_file():
                self.send_error(404)
                return
            content = path.read_bytes()
            self.send_response(200)
            self.send_header('Content-Type', self.guess_type(name))
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            self.wfile.write(content)
        def end_headers(self):
            self.send_header('Cache-Control', 'no-store')
            super().end_headers()
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    try:
        import threading
        server.daemon_threads = True
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        exercise(f'http://127.0.0.1:{server.server_port}/{OUTPUT.name}')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
