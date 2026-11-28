#!/usr/bin/env python3
"""Exercise the compiled Ziran website in a private Chromium session."""
import argparse
from contextlib import contextmanager
import base64
import functools
import hashlib
import http.server
import io
import json
import os
import re
from pathlib import Path
import select
import signal
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import urljoin, urlsplit

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'docs/site'
PAGES = ['index', 'getting-started', 'widgets', 'modules', 'apps', 'docs', 'renderers']
# The Apps page lists every app with its card image: web builds play in the
# browser, desktop apps say why they have none yet.
WEB_APPS = [('Werkzeug', 'werkzeug'), ('Inner Breeze', 'inner-breeze'), ('Jungle Academy', 'jungle'),
            ('Arcade', 'arcade'), ('Shovey', 'shovey'), ('Apfelwurm', 'apfelwurm'), ('Choano', 'choano')]
DESKTOP_APPS = [('Terminal', 'terminal'), ('Explorer', 'explorer'), ('Workbook', 'workbook'),
                ('Browser', 'browser'), ('Task Manager', 'taskmgr')]
VIEWPORTS = (320, 390, 768, 1024, 1366)


def maintained_scene_count():
    """How many scenes the gallery catalog builds from this checkout."""
    sys.path.insert(0, str(ROOT / 'scripts'))
    from gallery import prepare
    with tempfile.TemporaryDirectory() as scratch:
        _, entries = prepare(inventory=True)
    return len(entries)


# The site ships every maintained scene, however many there are.
EXPECTED_SCENE_COUNT = maintained_scene_count()


@contextmanager
def test_step(name, seconds=180):
    """Interrupt blocked browser I/O as well as waits, and identify the phase."""
    started = time.monotonic()
    print('Starting ' + name + ' (deadline ' + str(seconds) + 's)', flush=True)
    def expired(signum, frame):
        raise TimeoutError(name + ': exceeded ' + str(seconds) + 's')
    previous = signal.signal(signal.SIGALRM, expired)
    signal.setitimer(signal.ITIMER_REAL, seconds)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous)
    print('Passed ' + name + ' (' + str(round(time.monotonic() - started, 1)) + 's)', flush=True)


def run_step(name, function, *args, seconds=180):
    with test_step(name, seconds):
        return function(*args)


class DevToolsError(RuntimeError):
    def __init__(self, method, error):
        self.method, self.code, self.message = method, error.get('code'), str(error)
        super().__init__(method + ': ' + str(error))


class Browser:
    def __init__(self, profile, chromium):
        shutil.rmtree(profile, ignore_errors=True)
        profile.mkdir(parents=True)
        environment = {key: value for key, value in os.environ.items() if key not in (
            'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY', 'GDK_DISPLAY',
            'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION', 'DESKTOP_STARTUP_ID',
            'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'XDG_CURRENT_DESKTOP', 'XDG_SESSION_DESKTOP', 'XDG_RUNTIME_DIR')}
        environment.update(YUE_DESKTOP_RECOVERY='0', HARMONY_DESKTOP_RECOVERY='0')
        runtime_root = os.environ.get('KRYON_TEST_RUNTIME')
        self.runtime = tempfile.TemporaryDirectory(prefix='kryon-', dir=runtime_root) if runtime_root else None
        runtime_profile = Path(self.runtime.name) / 'profile' if self.runtime else profile
        temporary = Path(self.runtime.name) if self.runtime else ROOT / 'build/scratch/t'
        temporary.mkdir(parents=True, exist_ok=True)
        environment['TMPDIR'] = str(temporary)
        self.log = (profile / 'chromium.log').open('wb')
        self.process = subprocess.Popen(
            ['sh', '-c', 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', 'chromium', chromium,
             '--headless', '--no-sandbox', '--disable-gpu', '--use-gl=disabled',
             '--disable-dev-shm-usage', '--no-first-run', '--no-default-browser-check',
             '--hide-scrollbars', '--mute-audio', '--remote-debugging-pipe',
             '--user-data-dir=' + str(runtime_profile), 'about:blank'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=environment, start_new_session=True)
        os.set_blocking(self.process.stdin.fileno(), False)
        self.received, self.sequence, self.session, self.errors = b'', 0, None, []
        self.console_messages = []
        try:
            target = next(item for item in self.command('Target.getTargets')['targetInfos'] if item['type'] == 'page')
            self.session = self.command('Target.attachToTarget', targetId=target['targetId'], flatten=True)['sessionId']
            for domain in ('Runtime', 'Page', 'Log'):
                self.command(domain + '.enable')
        except Exception as error:
            self.close()
            with (profile / 'chromium.log').open('rb') as log:
                log.seek(0, os.SEEK_END)
                log.seek(max(0, log.tell() - 6000))
                diagnostics = log.read().decode(errors='replace')
            raise RuntimeError('Chromium startup failed: ' + str(error) + '\n' + diagnostics) from error

    def close(self):
        try: self.process.stdin.close()
        except OSError: pass
        try: os.killpg(self.process.pid, 15)
        except ProcessLookupError: pass
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(self.process.pid, 9); self.process.wait()
        self.process.stdout.close()
        self.log.close()
        if self.runtime: self.runtime.cleanup()

    def command(self, method, timeout=30, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session: message['sessionId'] = self.session
        deadline = time.monotonic() + timeout
        request = json.dumps(message).encode() + b'\0'
        sent = 0
        descriptor = self.process.stdin.fileno()
        while sent < len(request):
            remaining = deadline - time.monotonic()
            if remaining <= 0: raise TimeoutError(method + ': writing browser request')
            if select.select([], [descriptor], [], min(.2, remaining))[1]:
                try: sent += os.write(descriptor, request[sent:sent + 4096])
                except BlockingIOError: pass
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw: continue
                reply = json.loads(raw)
                event = reply.get('method')
                if event == 'Runtime.exceptionThrown':
                    details = reply['params']['exceptionDetails']
                    self.errors.append((details.get('exception') or {}).get('description') or details.get('text'))
                if event == 'Runtime.consoleAPICalled' and reply['params'].get('type') in ('error', 'warning'):
                    self.console_messages.append(str(reply['params'].get('args', []))[:1800])
                    self.console_messages = self.console_messages[-12:]
                if reply.get('id') == self.sequence:
                    if 'error' in reply: raise DevToolsError(method, reply['error'])
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data: raise RuntimeError('Chromium exited')
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression, timeout=30):
        result = self.command('Runtime.evaluate', expression=expression,
                              returnByValue=True, awaitPromise=True, timeout=timeout)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:1200])
        return result.get('result', {}).get('value')

    def until(self, expression, what, seconds=30):
        deadline = time.monotonic() + seconds
        value = None
        while time.monotonic() < deadline:
            try: value = self.evaluate(expression, timeout=max(.001, min(30, deadline - time.monotonic())))
            except TimeoutError as error:
                raise TimeoutError(what + ': ' + str(error)) from error
            except DevToolsError as error:
                if error.method != 'Runtime.evaluate' or error.code != -32000:
                    raise
            if value: return value
            time.sleep(.1)
        raise AssertionError(what + ': timed out at %r' % value)

    def viewport(self, width, height, mobile=False):
        self.command('Emulation.setDeviceMetricsOverride', width=width, height=height,
                     deviceScaleFactor=1, mobile=mobile)

    def open(self, url):
        self.command('Page.navigate', url=url)
        self.loaded("document.readyState === 'complete'", 'load ' + url)

    # Page.reload answers before the new document has run its scripts, so an
    # evaluation can still reach the old page or a new one without Module.
    # Only the new document lacks this mark.
    def reload(self):
        self.evaluate('window.testReloading = true')
        self.command('Page.reload')
        self.loaded("!window.testReloading && document.readyState === 'complete'", 'reload')

    def loaded(self, expression, what):
        self.until(expression, what)
        self.evaluate('''(async () => {
          await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
          return true;
        })()''')

    def key(self, key, keycode, modifiers=0):
        text = {'Enter': '\r', 'Escape': '\u001b'}.get(key)
        for kind in ('keyDown', 'keyUp'):
            params = {'type': kind, 'key': key, 'code': 'Key' + key.upper() if len(key) == 1 and key.isalpha() else key,
                      'windowsVirtualKeyCode': keycode, 'modifiers': modifiers}
            if text and kind == 'keyDown':
                params.update(text=text, unmodifiedText=text)
            self.command('Input.dispatchKeyEvent', **params)

        committed = self.evaluate('Module.siteFrame || 0')
        self.until('Module.siteFrame >= ' + str(committed + 2), 'key event committed')

    def reduced_motion(self, enabled):
        self.command('Emulation.setEmulatedMedia', features=[
            {'name': 'prefers-reduced-motion', 'value': 'reduce' if enabled else 'no-preference'}])

    def screenshot(self, path):
        result = self.command('Page.captureScreenshot', format='png')
        path.write_bytes(base64.b64decode(result['data']))


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        if urlsplit(self.path).path != '/compiler-worker-fixture.js':
            return super().do_GET()
        # Inspect the real worker's FS without adding a product test protocol.
        script = (Path(self.directory) / 'compiler-worker.js').read_text() + r'''
const compileMessage = self.onmessage;
self.onmessage = event => {
  const data = event.data;
  if (!data.fixture) { compileMessage(event); return; }
  pending = pending.then(async () => {
    const compiler = await runtime;
    if (data.fixture === 'seed') {
      compiler.FS.mkdirTree('/gallery/examples');
      compiler.FS.writeFile('/gallery/examples/native.tsx', data.source);
      return;
    }
    const intact = compiler.FS.readFile('/gallery/examples/native.tsx', {encoding:'utf8'}) === data.source;
    const clean = !compiler.FS.analyzePath('/examples/gallery/src/examples/native.tsx').exists &&
                  !compiler.FS.analyzePath('/preview.zx').exists;
    self.postMessage({id:data.id, status:intact && clean ? 0 : 1,
      diagnostics:JSON.stringify({intact, clean})});
  });
};
'''
        data = script.encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/javascript')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)
    def translate_path(self, path):
        # Like Cloudflare Pages, serve widgets.html at the clean /widgets path.
        local = super().translate_path(path)
        if not os.path.splitext(local)[1] and os.path.isfile(local + '.html'):
            return local + '.html'
        return local
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


class QuietServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, request, client_address):
        if isinstance(sys.exc_info()[1], (BrokenPipeError, ConnectionResetError)): return
        super().handle_error(request, client_address)


def serve(site):
    server = QuietServer(('127.0.0.1', 0), functools.partial(QuietHandler, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def tree(browser):
    return browser.evaluate('Module.siteTree || []')


def visible(node, width=1366, height=844):
    left = max(node['x'], node['clipX'] if node.get('clipped', False) else 0, 0)
    top = max(node['y'], node['clipY'] if node.get('clipped', False) else 0, 0)
    right = min(node['x'] + node['width'], node['clipX'] + node['clipWidth'] if node.get('clipped', False) else width, width)
    bottom = min(node['y'] + node['height'], node['clipY'] + node['clipHeight'] if node.get('clipped', False) else height, height)
    return right > left and bottom > top


def node_for(browser, *, key=None, label=None, value=None, width=1366, parent_key=None):
    nodes = tree(browser)
    parents = [i for i, n in enumerate(nodes) if n['key'] == parent_key] if parent_key is not None else []
    if parent_key is not None:
        assert len(parents) == 1, ('Navigation parent', parent_key, parents)
    matches = [n for n in nodes if
               (parent_key is None or n['parent'] == parents[0]) and
               (key is None or n['key'] == key) and
               (label is None or n['label'] == label) and
               (value is None or value in n.get('value', '')) and visible(n, width)]
    assert matches, (key, label, value, [(n['key'], n['label']) for n in tree(browser) if n['interactive']][:20])
    if parent_key is not None:
        assert len(matches) == 1 and matches[0]['interactive'], ('Navigation item', matches)
    return matches[-1]


def point(node, width):
    left, top = max(node['x'], node['clipX'] if node.get('clipped', False) else 0, 0), max(node['y'], node['clipY'] if node.get('clipped', False) else 0, 0)
    right = min(node['x'] + node['width'], node['clipX'] + node['clipWidth'] if node.get('clipped', False) else width, width)
    bottom = min(node['y'] + node['height'], node['clipY'] + node['clipHeight'] if node.get('clipped', False) else 844, 844)
    assert right > left and bottom > top, node
    return (left + right) / 2, (top + bottom) / 2


def mouse(browser, node, width=1366, click=True):
    x, y = point(node, width)
    browser.command('Input.dispatchMouseEvent', type='mouseMoved', x=x, y=y)
    if click:
        browser.command('Input.dispatchMouseEvent', type='mousePressed', x=x, y=y, button='left', clickCount=1)
        time.sleep(.04)
        browser.command('Input.dispatchMouseEvent', type='mouseReleased', x=x, y=y, button='left', clickCount=1)
        committed = browser.evaluate('Module.siteFrame || 0')
        browser.until('Module.siteFrame >= ' + str(committed + 2), 'pointer event committed')
    return x, y


def ready(browser, base, route='widgets', width=1366, hash=''):
    browser.viewport(width, 844)
    browser.open(base + route + '.html?test=1' + hash)
    browser.until('Module.galleryReady === 1 && Module.siteTree?.length > 0', 'compiled Ziran ready', 60)


def capture(browser, path):
    data = browser.command('Page.captureScreenshot', format='png')['data']
    with Image.open(io.BytesIO(base64.b64decode(data))).convert('RGB') as image:
        image.thumbnail((1120, 844))
        assert max(channel[1] - channel[0] for channel in image.getextrema()) > 50, 'Blank paint'
        image.save(path, 'WEBP', quality=78, method=4)


def assert_picture_pixels(image, box, name):
    crop = image.crop(box).convert('RGB').resize((64, 64))
    pixels = list(crop.getdata())
    colors = {(r // 16, g // 16, b // 16) for r, g, b in pixels}
    saturated = sum(max(pixel) - min(pixel) > 24 for pixel in pixels)
    assert len(colors) > 48 and saturated > 80, (name, 'image did not paint', len(colors), saturated)


def check_picture(browser, name, shots):
    nodes = [node for node in tree(browser) if node['kind'] == 24 and visible(node, 1366)]
    assert nodes, (name, 'no visible image widget')
    node = max(nodes, key=lambda n: n['width'] * n['height'])
    left, top = max(0, node['x']), max(0, node['y'])
    right, bottom = min(1366, node['x'] + node['width']), min(844, node['y'] + node['height'])
    box = (left + (right-left)*.15, top + (bottom-top)*.15, right - (right-left)*.15, bottom - (bottom-top)*.15)
    time.sleep(.2)
    data = browser.command('Page.captureScreenshot', format='png')['data']
    with Image.open(io.BytesIO(base64.b64decode(data))) as image:
        assert_picture_pixels(image, box, name)
        image.convert('RGB').save(shots / (name.lower().replace(' ', '-') + '.webp'), 'WEBP', quality=82)


def check_files(site):
    maintained = {name + '.html' for name in PAGES}
    assert {path.name for path in site.rglob('*.html')} == maintained, 'Unmaintained website page'
    for name in PAGES:
        source = (site / (name + '.html')).read_text()
        assert source.count('<canvas ') == 1 and '<iframe' not in source
        assert 'current.js' in source and 'id="kryon-ime"' in source
        assert '<button' not in source and '<select' not in source, 'Website controls must be Ziran'
        for filename in ('favicon.ico', 'favicon-32.png', 'apple-touch-icon.png'):
            revision = hashlib.sha256((site / filename).read_bytes()).hexdigest()[:12]
            assert f'href="/{filename}?v={revision}"' in source, 'Missing or stale mascot favicon'
        assert 'property="og:image" content="https://kryonlabs.com/og.png"' in source
    assert (site / 'runtime/simd128.js').stat().st_size > 10000
    assert (site / 'runtime/baseline.js').stat().st_size > 10000
    assets = dict(json.loads((site / 'runtime/assets.json').read_text()))
    image = site / assets['/assets/pfp.webp']
    assert hashlib.sha256(image.read_bytes()).digest() == hashlib.sha256((ROOT / 'examples/gallery/assets/pfp.webp').read_bytes()).digest(), 'Gallery image asset missing or changed'
    mascot = site / assets['/assets/brand/mascot-dragon.png']
    canonical_mascot = ROOT / 'assets/brand/mascot-dragon.png'
    assert mascot.read_bytes() == canonical_mascot.read_bytes(), 'Kryon mascot asset missing or changed'
    assert (site / 'assets/brand/mascot-dragon.png').read_bytes() == canonical_mascot.read_bytes()
    assert '/assets/apps/github.webp' not in assets, 'Retired GitHub client demo was packaged'
    landscape = site / assets['/assets/alpine-lake.webp']
    assert hashlib.sha256(landscape.read_bytes()).digest() == hashlib.sha256((ROOT / 'examples/image/assets/alpine-lake.webp').read_bytes()).digest()
    for app in WEB_APPS + DESKTOP_APPS:
        image = site / assets['/assets/apps/' + app[1] + '.webp']
        assert image.stat().st_size > 2000, ('app card image missing', app)
    coast = site / assets['/assets/coastal-cliffs.webp']
    assert coast.read_bytes() == (ROOT / 'examples/carousel/assets/coastal-cliffs.webp').read_bytes()
    assert coast.read_bytes() != landscape.read_bytes(), 'Carousel must ship two distinct landscapes'
    text = (site / 'gallery-data.js').read_text()
    scenes = json.loads(text.split('window.galleryScenes = ', 1)[1].rstrip(';\n'))
    assert len(scenes) == EXPECTED_SCENE_COUNT
    return scenes


def compile_ready(browser, seconds=120):
    browser.until('Module.siteTree?.some(n => n.key === 10102)', 'source editor stays selected')
    browser.until('!Module.compileBusy && Module.compileRevision === Module.editorRevision', 'compiler completion', seconds)
    diagnostic = browser.evaluate('Module.compileDiagnostic')
    assert not diagnostic, diagnostic
    browser.until('Module.previewReady && Module.previewNodes > 0', 'portable preview paint', 30)


def replace_source(browser, text):
    revision = browser.evaluate("Module.editorRevision")
    assert text != node_for(browser, key=10102)['value'], 'Source replacement must change the text'
    mouse(browser, node_for(browser, key=10102))
    browser.command('Browser.grantPermissions', origin=browser.evaluate('location.origin'),
                    permissions=['clipboardReadWrite', 'clipboardSanitizedWrite'])
    browser.evaluate('navigator.clipboard.writeText(' + json.dumps(text) + ')')
    browser.key('a', 65, modifiers=2)
    browser.key('v', 86, modifiers=2)
    browser.until('Module.editorRevision > ' + str(revision), 'trusted source edit')


def start_compiler(browser, base, worker='compiler-worker.js'):
    browser.open(base + 'compiler-worker.js')
    browser.evaluate('window.compilerWorker = new Worker(' + json.dumps(worker) + ');' + """
      window.compilerReply = null;
      window.compilerReplies = [];
      compilerWorker.onmessage = ({data}) => { window.compilerReply = {id:data.id, status:data.status,
        diagnostics:data.diagnostics, bytes:data.bytes?.byteLength || 0};
        compilerReplies.push(compilerReply); };
      compilerWorker.onerror = e => { window.compilerReply = {status:1, diagnostics:e.message}; };""")
    sys.path.insert(0, str(ROOT / 'scripts'))
    from gallery import compiler_paths, enabled_modules
    return '\n'.join(compiler_paths(enabled_modules()))


def check_native_tsx_compiler(browser, base):
    search = start_compiler(browser, base, 'compiler-worker-fixture.js')
    source = ('import * as App from "kryon/app";\n'
              'import * as Text from "kryon/text";\n'
              'interface Props { text: string; }\n'
              'function Label(props: Props): string { return props.text; }\n'
              'export function Frame(frame: App.Frame): void {\n'
              '    Text.Draw(frame, {key: u64(60001n), bounds: frame.viewport,\n'
              '        text: <Label text={"Native TSX"}/>});\n'
              '}\n')
    path = 'gallery/examples/native.tsx'
    requests = [source, source.replace('text={"Native TSX"}', 'misspelled={"Native TSX"}'),
                'export function Frame(frame: s32): void {}\n', source]
    try:
        browser.evaluate('compilerWorker.postMessage(' + json.dumps({
            'id': 0, 'source': source, 'sourcePath': 'gallery/examples/warmup.tsx', 'search': search}) + ');')
        warmup = browser.until('compilerReplies.length === 1 && compilerReplies[0]', 'native TSX runtime startup', 120)
        assert warmup['status'] == 0 and warmup['bytes'] > 0, warmup
        original = '// bundled source must survive edits\n' + source
        browser.evaluate('compilerReplies = []; compilerWorker.postMessage(' + json.dumps({
            'fixture': 'seed', 'source': original}) + ');')
        # Requests stay ordered; failures cannot return previous bundle bytes
        # or delete a maintained source from the shared compiler filesystem.
        browser.evaluate('for (const request of ' + json.dumps([
            {'id': index + 1, 'source': code,
             'sourcePath': path if index < 3 else 'examples/gallery/src/examples/native.tsx',
             'search': search}
            for index, code in enumerate(requests)]) + ') compilerWorker.postMessage(request);')
        replies = browser.until('compilerReplies.length === 4 && compilerReplies', 'queued native TSX compilation', 240)
        assert [reply['id'] for reply in replies] == [1, 2, 3, 4], replies
        for index in (0, 3):
            assert replies[index]['status'] == 0 and replies[index]['bytes'] > 0, replies[index]
        for index in (1, 2):
            assert replies[index]['status'] != 0 and replies[index]['bytes'] == 0, replies[index]
            assert replies[index]['diagnostics'].strip(), replies[index]
        assert re.search(re.escape(path) + r':7:\d+', replies[1]['diagnostics']), replies[1]
        assert 'misspelled' in replies[1]['diagnostics'], replies[1]
        assert 'host provider signature mismatch' in replies[2]['diagnostics'].lower(), replies[2]
        browser.evaluate('compilerWorker.postMessage(' + json.dumps({
            'id': 5, 'fixture': 'inspect', 'source': original}) + ');')
        cleanup = browser.until('compilerReplies.length === 5 && compilerReplies[4]', 'compiler source restoration', 30)
        assert cleanup['status'] == 0, cleanup
    finally:
        browser.evaluate('compilerWorker.terminate()')
    print('Local browser native TSX test passed: typed bindings, authored diagnostics, queued recovery and source restoration')


def check_compiler(browser, base, scenes):
    check_native_tsx_compiler(browser, base)
    search = start_compiler(browser, base)
    try:
        for index, scene in enumerate(scenes):
            print('Compiling source ' + str(index + 1) + '/' + str(len(scenes)) + ': ' + scene['name'], flush=True)
            request = {'id': index + 1, 'sourcePath': scene['sourcePath'], 'search': search}
            browser.evaluate('fetch(' + json.dumps(scene['code']) + ').then(r => { if (!r.ok) throw new Error("Missing maintained TSX"); return r.text(); }).then(source => { window.compilerReply = null; compilerWorker.postMessage({...'
                             + json.dumps(request) + ',source}); })')
            reply = browser.until('window.compilerReply', 'source compile ' + scene['name'], 120)
            assert reply['status'] == 0 and reply['bytes'] > 0, (scene['name'], reply)
    finally:
        browser.evaluate('compilerWorker.terminate()')
    print('Local browser compiler test passed: all ' + str(len(scenes)) + ' maintained source variants compile to portable bundles')


def frame_crop(browser, node):
    data = browser.command('Page.captureScreenshot', format='png')['data']
    image = Image.open(io.BytesIO(base64.b64decode(data))).convert('RGB')
    return image.crop((int(node['x']), int(node['y']), int(node['x'] + node['width']), int(node['y'] + node['height'])))


def assert_text_contrast(image, label):
    from collections import Counter
    def luminance(color):
        values = [v / 255 for v in color]
        values = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in values]
        return sum(v * w for v, w in zip(values, (.2126, .7152, .0722)))
    colors = Counter(image.getdata())
    background = luminance(colors.most_common(1)[0][0])
    readable = sum(count for color, count in colors.items()
                   if (max(background, luminance(color)) + .05) / (min(background, luminance(color)) + .05) >= 4.5)
    assert readable >= 12, (label, 'unreadable text', colors.most_common(4))


def check_segment_contrast(browser):
    for label in ('Preview', 'Source'):
        image = frame_crop(browser, node_for(browser, label=label))
        assert_text_contrast(image.crop((8, 6, image.width - 8, image.height - 6)), label)


def check_portable_theme(browser, shots):
    mouse(browser, node_for(browser, label='Source'))
    compile_ready(browser)
    for dark in (True, False):
        if browser.evaluate('Module.siteDark') != dark:
            mouse(browser, node_for(browser, key=10003))
        browser.until('Module.siteDark === ' + str(dark).lower(), 'portable theme publication')
        frame = browser.evaluate('Module.siteFrame')
        browser.until('Module.siteFrame > ' + str(frame + 3), 'guest themed frames')
        heading = browser.evaluate("Module.previewTree.find(n => n.key === 1 && n.label === 'Button')")
        assert_text_contrast(frame_crop(browser, heading), 'portable heading ' + str(dark))
        capture(browser, shots / ('portable-dark.webp' if dark else 'portable-light.webp'))
    mouse(browser, node_for(browser, key=10003))
    browser.until('Module.siteDark', 'restore dark theme')


def navigation_scenes(scenes):
    return sorted(scenes, key=lambda e: (e['moduleId'] not in ('kryon', 'inputs', 'overlays'), '' if e['moduleId'] in ('kryon', 'inputs', 'overlays') else e['module'], e['category'], e['name']))


def select_example(browser, base, scenes, name, reload=True):
    if reload: ready(browser, base)
    nodes = tree(browser)
    nav = next(n for n in nodes if n['key'] == 9003)
    selected = next(n for n in nodes if n['selected'] and n['parent'] == nodes.index(nav))
    mouse(browser, selected)
    browser.key('Home', 36)
    ordered = navigation_scenes(scenes)
    position = next(i for i, scene in enumerate(ordered) if scene['name'] == name)
    index = ordered[position]['index']
    if position > len(scenes) // 2:
        browser.key('End', 35)
        for _ in range(len(scenes) - 1 - position): browser.key('ArrowUp', 38)
    else:
        for _ in range(position): browser.key('ArrowDown', 40)
    browser.until('Module.gallerySceneRendered === ' + str(index), 'options example ' + name)


def check_example_options(browser, base, scenes, shots):
    ready(browser, base)
    if not browser.evaluate('Module.siteDark'): mouse(browser, node_for(browser, key=10003))
    check_segment_contrast(browser)
    capture(browser, shots / 'segment-dark.webp')
    check_portable_theme(browser, shots)
    select_example(browser, base, scenes, 'Empty state')
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.siteTree.some(n => n.label === 'Create project' && n.disabled)", 'native disabled empty action')
    mouse(browser, node_for(browser, label='Create project'))
    assert not any(n['label'] == 'Project created' for n in tree(browser))
    select_example(browser, base, scenes, 'Copy button', reload=False)
    assert node_for(browser, label='Copy website address')['disabled'], 'disabled flag lost on scene change'
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.siteTree.some(n => n.label === 'Copy website address' && !n.disabled)", 'native reenabled copy')
    for portable in (False, True):
        select_example(browser, base, scenes, 'Carousel')
        if portable:
            mouse(browser, node_for(browser, label='Source'))
            compile_ready(browser)
        collection = 'Module.previewTree' if portable else 'Module.siteTree'
        mouse(browser, node_for(browser, key=9004))
        browser.until(collection + ".some(n => n.label === 'Next' && n.disabled)", 'disabled carousel navigation')
        arrow = browser.evaluate(collection + ".find(n => n.label === 'Next')")
        action_kind = str(arrow['kind'])
        mouse(browser, arrow)
        assert browser.evaluate(collection + ".some(n => n.label === 'Open' && n.disabled && n.kind === " + action_kind + ")"), 'disabled carousel navigated'
        mouse(browser, node_for(browser, key=9004))
        browser.until(collection + ".some(n => n.label === 'Next' && !n.disabled)", 'reenabled carousel navigation')
        mouse(browser, browser.evaluate(collection + ".find(n => n.label === 'Next')"))
        browser.until(collection + ".some(n => n.label === 'Create' && !n.disabled && n.kind === " + action_kind + ")", 'reenabled carousel action advances')
    select_example(browser, base, scenes, 'Empty state')
    mouse(browser, node_for(browser, label='Source'))
    compile_ready(browser)
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.previewTree.some(n => n.label === 'Create project' && n.disabled)", 'portable disabled empty action')
    mouse(browser, browser.evaluate("Module.previewTree.find(n => n.label === 'Create project')"))
    assert not browser.evaluate("Module.previewTree.some(n => n.label === 'Project created')")
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.previewTree.some(n => n.label === 'Create project' && !n.disabled)", 'portable reenabled empty action')
    mouse(browser, browser.evaluate("Module.previewTree.find(n => n.label === 'Create project')"))
    browser.until("Module.previewTree.some(n => n.label === 'Project created')", 'reenabled empty action fires')
    select_example(browser, base, scenes, 'Button', reload=False)
    compile_ready(browser)
    button = browser.evaluate("Module.previewTree.find(n => n.label === 'Continue' && !n.disabled)")
    mouse(browser, button)
    browser.until("Module.previewTree.some(n => n.key === 2 && n.label === '1')", 'portable old example enabled')
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.previewTree.some(n => n.label === 'Continue' && n.disabled)", 'portable old example disabled')
    mouse(browser, button)
    assert browser.evaluate("Module.previewTree.some(n => n.key === 2 && n.label === '1')"), 'disabled button activated'
    select_example(browser, base, scenes, 'Toggle', reload=False)
    compile_ready(browser)
    browser.until("Module.previewTree.some(n => n.label === 'Notifications' && n.disabled)", 'disabled WidgetInput survives scene change')
    mouse(browser, node_for(browser, key=9004))
    browser.until("Module.previewTree.some(n => n.label === 'Notifications' && !n.disabled)", 'WidgetInput reenabled without leakage')
    browser.reduced_motion(True)
    select_example(browser, base, scenes, 'Loading indicator')
    browser.until('Module.siteReducedMotion', 'actual reduced motion media')
    def unchanged(node, expected):
        first = frame_crop(browser, node)
        frame = browser.evaluate('Module.siteFrame')
        browser.until('Module.siteFrame > ' + str(frame + 18), 'animation observation')
        equal = ImageChops.difference(first, frame_crop(browser, node)).getbbox() is None
        assert equal == expected, ('loading animation', equal, expected)
    unchanged(node_for(browser, key=10101), True)
    capture(browser, shots / 'reduced-motion.webp')
    browser.reduced_motion(False)
    browser.until('!Module.siteReducedMotion', 'actual normal motion media')
    unchanged(node_for(browser, key=10101), False)
    mouse(browser, node_for(browser, label='Source'))
    compile_ready(browser)
    heading = browser.evaluate("Module.previewTree.find(n => n.key === 1 && n.label === 'Loading indicator')")
    area = dict(x=heading['x'], y=heading['y'] + 60, width=heading['width'], height=180)
    unchanged(area, False)
    browser.reduced_motion(True)
    browser.until('Module.siteReducedMotion', 'portable reduced motion enabled')
    frame = browser.evaluate('Module.siteFrame')
    browser.until('Module.siteFrame > ' + str(frame + 3), 'portable option publication')
    unchanged(area, True)
    browser.reduced_motion(False)



def check_preview_containment(browser, base, shots):
    for width in (1366, 390):
        ready(browser, base, 'widgets', width=width)
        stage = node_for(browser, key=10101, width=width)
        nodes = tree(browser)
        actions = [n for n in nodes if n['label'] == 'Continue' and n['interactive']]
        assert len(actions) == 12
        assert all(n['sessionSlot'] != node_for(browser, key=10100, width=width)['sessionSlot'] for n in actions), 'Native preview must keep its own actual Session'
        assert all(n['y'] >= stage['y'] and n['y'] + n['height'] <= stage['y'] + stage['height'] for n in actions), 'Maintained Button rows overflow stage'
        assert all(n['clipped'] and n['clipY'] >= stage['y'] and n['clipY'] + n['clipHeight'] <= stage['y'] + stage['height'] for n in actions), 'Native example lacks stage clip'
        usage = next(n for n in nodes if n['key'] == 10304)
        assert usage['y'] > stage['y'] + stage['height'], 'Usage overlaps example'
        mouse(browser, node_for(browser, label='Source', width=width), width=width)
        compile_ready(browser)
        overflow = ('import * as App from "kryon/app";\n'
                    'import { ColorFromPacked } from "kryon/drawing_props";\n'
                    'import { Session } from "kryon/session";\n'
                    'import { Box } from "kryon/tsx/box";\n'
                    'export function Frame(frame: App.Frame): void {\n'
                    ' const viewport = frame.viewport;\n'
                    ' const session: Session = {slot: frame.slot, epoch: frame.epoch};\n'
                    ' <Box session={session} props={{key: u64(771), bounds: {x: viewport.x - 80, '
                    'y: viewport.y - 80, width: viewport.width + 160, height: viewport.height + 160}, '
                    'fill: ColorFromPacked(u32(0xF700DFFF))}}/>;\n}\n'
                    'export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {}\n')
        replace_source(browser, overflow)
        compile_ready(browser)
        header_height = 56 if width >= 760 else 104
        header = frame_crop(browser, dict(x=0,y=0,width=width,height=header_height))
        browser.command('Input.dispatchMouseEvent', type='mouseWheel', x=width-4, y=620, deltaX=0, deltaY=610)
        browser.until('Module.siteTree.some(n=>n.key===10100 && n.y<0)', 'overflow preview article scroll')
        time.sleep(.5)
        guest = browser.evaluate('Module.previewTree.find(n=>n.key===771)')
        bounds = dict(x=guest['x']+80,y=guest['y']+80,width=guest['width']-160,height=guest['height']-160)
        pixels = frame_crop(browser, dict(x=0,y=0,width=width,height=844))
        colored = [(x,y) for y in range(844) for x in range(width) if (lambda p:p[0]>230 and p[1]<30 and p[2]>200)(pixels.getpixel((x,y)))]
        assert len(colored)>3000, 'Real oversized guest did not paint'
        assert all(bounds['x'] <= x < bounds['x']+bounds['width'] and max(header_height,bounds['y']) <= y < bounds['y']+bounds['height'] for x,y in colored), 'Guest paint escaped preview intersection article'
        assert ImageChops.difference(header, frame_crop(browser, dict(x=0,y=0,width=width,height=header_height))).getbbox() is None, 'Guest clip scope changed header pixels'
        capture(browser, shots / ('overflow-contained-' + str(width) + '.webp'))


def check_clean_routes(browser, base):
    # Cloudflare Pages redirects widgets.html to /widgets; each clean path must keep its page.
    for page, name in enumerate(('index', 'getting-started', 'widgets', 'apps', 'docs', 'renderers', 'modules')):
        if page == 0: continue
        browser.viewport(1366, 844)
        browser.open(base + name + '?test=1')
        browser.until('Module.galleryReady === 1 && Module.siteTree?.length > 0', 'compiled Ziran ready at /' + name, 60)
        shown = browser.evaluate('Module.sitePage')
        assert shown == page, ('Clean route opened the wrong page', '/' + name, shown)
    print('Local clean-route test passed: every page opens at its extensionless path')


def check_document_layout(browser, base, shots):
    ready(browser, base, 'widgets', width=1366)
    title = node_for(browser, key=10100)
    rail = node_for(browser, key=9003)
    assert abs(rail['x'] - 12) < 1 and abs(rail['width'] - 240) < 1, rail
    assert title['x'] == 312 and title['width'] <= 820, title
    mouse(browser, node_for(browser, key=10502))
    browser.until('Module.siteTree.some(n => n.key===10304 && n.y<160)', 'Usage contents link scrolls article')
    assert node_for(browser, key=10309)['readOnly'], 'Usage must show maintained source'
    mouse(browser, node_for(browser, key=10501))
    browser.until('Module.siteTree.some(n => n.key===10100 && n.y>100)', 'Example contents link restores article')
    capture(browser, shots/'documentation-wide.webp')
    ready(browser, base, 'index', width=390)
    assert not any(n['key']==9003 for n in tree(browser)), 'Overview must remain a landing page'
    mascot = node_for(browser, key=10201, width=390)
    assert mascot['kind'] == 24 and visible(mascot, 390), 'Mascot must paint in the overview hero'
    capture(browser, shots/'overview-phone.webp')
    mouse(browser, node_for(browser, key=10520, width=390), width=390)
    mouse(browser, node_for(browser, key=10524, width=390), width=390)
    browser.until('Module.sitePage===5', 'phone resources route')
    ready(browser, base, 'widgets', width=390)
    mouse(browser, node_for(browser, label='Source', width=390), width=390)
    compile_ready(browser)
    mouse(browser, node_for(browser, key=10103, width=390), width=390)
    browser.until('Module.siteTree.some(n => n.key===9003)', 'mobile source navigation')
    nodes = tree(browser)
    sidebar = next(i for i, node in enumerate(nodes) if node['key'] == 9003)
    target = next(node for node in nodes if node['parent'] == sidebar and
                  node['interactive'] and not node['selected'] and visible(node, width=390))
    expected_scene = target['key'] - 1
    assert expected_scene != browser.evaluate('Module.gallerySceneRendered')
    mouse(browser, target, width=390)
    browser.until('Module.gallerySceneRendered===' + str(expected_scene), 'mobile source changes real scene')
    compile_ready(browser)
    capture(browser, shots/'documentation-phone-source.webp')
    header = frame_crop(browser, dict(x=0,y=0,width=390,height=104))
    browser.command('Input.dispatchMouseEvent',type='mouseWheel',x=382,y=620,deltaX=0,deltaY=720)
    browser.until('Module.siteTree.some(n => n.key===10100 && n.y<0)', 'phone article scroll')
    time.sleep(.3)
    assert ImageChops.difference(header, frame_crop(browser, dict(x=0,y=0,width=390,height=104))).getbbox() is None, 'Guest preview painted over navigation'
    capture(browser, shots/'documentation-phone-scrolled.webp')



def check_scroll_drag(browser, base):
    ready(browser, base)
    title = node_for(browser, key=10100)
    page = next(n for n in tree(browser) if n['key'] == 10300)
    rail = next(n for n in tree(browser) if n['key'] == 9003)
    first_row = next(n for n in tree(browser) if n['key'] == 1 and n['parent'] >= 0 and
                     tree(browser)[n['parent']]['key'] == 9003)

    def drag(x, y, distance):
        browser.command('Input.dispatchMouseEvent', type='mouseMoved', x=x, y=y)
        browser.command('Input.dispatchMouseEvent', type='mousePressed', x=x, y=y,
                        button='left', buttons=1, clickCount=1)
        for step in range(1, 7):
            browser.command('Input.dispatchMouseEvent', type='mouseMoved', x=x,
                            y=y + distance * step / 6, button='left', buttons=1)
            time.sleep(.04)
        browser.command('Input.dispatchMouseEvent', type='mouseReleased', x=x,
                        y=y + distance, button='left', buttons=0, clickCount=1)

    drag(page['x'] + page['width'] - 5, page['y'] + 4, 200)
    browser.until('Module.siteTree.some(n => n.key===10100 && n.y < ' +
                  str(title['y'] - 100) + ')', 'page scrollbar thumb drag')
    drag(rail['x'] + rail['width'] - 5, rail['y'] + 40, 160)
    browser.until('Module.siteTree.some(n => n.key===1 && n.parent>=0 && ' +
                  'Module.siteTree[n.parent].key===9003 && n.y < ' +
                  str(first_row['y'] - 60) + ')', 'navigation scrollbar thumb drag')
    before = next(n for n in tree(browser) if n['key'] == 10100)['y']
    drag(1200, 500, -240)
    browser.until('Module.siteTree.some(n => n.key===10100 && n.y < ' +
                  str(before - 40) + ')', 'page content drag')


def check_apps(browser, base, shots):
    ready(browser, base, 'apps')
    nodes = tree(browser)
    images = [n for n in nodes if n['kind'] == 24 and visible(n, 1366)]
    assert {'Terminal', 'Explorer', 'Workbook'} <= {n['label'] for n in images}, 'Featured desktop cards missing'
    labels = {n['label'] for n in nodes}
    for name, _ in WEB_APPS + DESKTOP_APPS:
        assert name in labels, (name, sorted(labels))
    assert 'Shelf' not in labels, 'Explorer must not be called Shelf'
    assert labels.isdisjoint({'GitHub', 'KryD', 'Krait'}), 'Retired app listed on the website'
    assert not any('in development' in (n['label'] or '') for n in nodes), 'Apps must not be labelled in development'
    assert sum('Play in browser' in (n['label'] or '') for n in nodes) == len(WEB_APPS), 'Each web app opens its browser build'
    assert sum('View source' in (n['label'] or '') for n in nodes) == len(DESKTOP_APPS), 'Each desktop app links to its source'
    capture(browser, shots / 'apps.webp')


def check_live(browser, base, scenes, shots):
    ready(browser, base, 'index')
    mouse(browser, node_for(browser, key=10038))
    browser.until('Module.siteCount === 1', 'real counter click')
    mouse(browser, node_for(browser, key=10003))
    browser.until('Module.siteDark', 'theme toggle')
    browser.reload()
    browser.until('Module.galleryReady && Module.siteDark', 'persisted theme', 60)
    ready(browser, base)
    assert sum(n['label'] == 'Continue' and n['interactive'] for n in tree(browser)) == 12, 'default web differs from maintained Button'
    mouse(browser, node_for(browser, label='Continue', key=10))
    browser.until("Module.siteTree.some(n => n.key === 2 && n.label === '1')", 'default maintained Button click')
    select_example(browser, base, scenes, 'Button emphasis', reload=False)
    assert sum(n['label'] == 'Continue' and n['interactive'] for n in tree(browser)) == 12
    assert any(n['key'] == 2 and n['label'] == '1' for n in tree(browser)), 'variant lost source module state'
    select_example(browser, base, scenes, 'Toggle', reload=False)
    assert any(n['label'] == 'Notifications' for n in tree(browser))
    assert not any(n['label'] == 'Continue' for n in tree(browser)), 'distinct example modules leaked content'
    select_example(browser, base, scenes, 'Button', reload=False)
    assert any(n['key'] == 2 and n['label'] == '1' for n in tree(browser)), 'cross-scene source state lost'
    ready(browser, base)
    selected = next(n for n in tree(browser) if n['selected'] and n['parent'] >= 0 and tree(browser)[n['parent']]['key'] == 9003)
    mouse(browser, selected)
    browser.key('Home', 36)
    for position, scene in enumerate(navigation_scenes(scenes)):
        index = scene['index']
        if position: browser.key('ArrowDown', 40)
        browser.until('Module.gallerySceneRendered === ' + str(index), 'sidebar ' + scene['name'])
        browser.until('Module.siteTree?.some(n => n.key === 10100 && n.label === ' + json.dumps(scene['name']) + ')', 'committed title ' + scene['name'])
        assert any(n['focused'] and n['key'] == index + 1 and n['parent'] >= 0 and tree(browser)[n['parent']]['key'] == 9003 for n in tree(browser)), ('sidebar focus lost', scene['name'])
        if scene['name'] in ('Image', 'Carousel', 'Tile map'): check_picture(browser, scene['name'], shots)
    capture(browser, shots / 'components.webp')
    for width in VIEWPORTS:
        ready(browser, base, width=width)
        for key in (10002, 10003, 10012, 10104):
            assert visible(node_for(browser, key=key, width=width), width), (width, key)
        if width < 760:
            mouse(browser, node_for(browser, key=10103, width=width), width)
            target = navigation_scenes(scenes)[1]
            mouse(browser, node_for(browser, key=target['index'] + 1, label=target['name'], width=width, parent_key=9003), width)
            browser.until('Module.gallerySceneRendered === ' + str(target['index']), 'mobile picker')
        capture(browser, shots / ('components-' + str(width) + '.webp'))
    ready(browser, base)
    mouse(browser, node_for(browser, label='Source'))
    for position, scene in enumerate(navigation_scenes(scenes)):
        index = scene['index']
        if position:
            nav = node_for(browser, key=9003)
            selected = next(n for n in tree(browser) if n['selected'] and n['parent'] == tree(browser).index(nav))
            mouse(browser, selected)
            browser.key('ArrowDown', 40)
            browser.until('Module.gallerySceneRendered === ' + str(index), 'source selection')
        print('Compiling source ' + str(position + 1) + '/' + str(len(scenes)) + ': ' + scene['name'], flush=True)
        compile_ready(browser)
    ready(browser, base)
    mouse(browser, node_for(browser, label='Source'))
    compile_ready(browser)
    text = node_for(browser, key=10102)['value']
    assert '<Button ' in text and 'Continue' in text
    assert browser.evaluate("Module.previewTree.filter(n => n.label === 'Continue' && n.interactive).length === 12"), 'portable differs from maintained/default Button'
    mouse(browser, node_for(browser, key=10102))
    browser.evaluate("window.sourceKeys=[]; window.sourceKeyListener=e=>{if(e.key==='x')sourceKeys.push({type:e.type,trusted:e.isTrusted})}; for(const kind of ['keydown','keypress','keyup'])document.addEventListener(kind,sourceKeyListener,true)")
    revision = browser.evaluate('Module.editorRevision')
    browser.command('Input.dispatchKeyEvent', type='keyDown', key='x', code='KeyX',
                    windowsVirtualKeyCode=88, text='x', unmodifiedText='x')
    browser.command('Input.dispatchKeyEvent', type='keyUp', key='x', code='KeyX', windowsVirtualKeyCode=88)
    browser.until('Module.editorRevision > ' + str(revision), 'trusted keyboard source edit')
    assert browser.evaluate('sourceKeys') == [{'type': kind, 'trusted': True} for kind in ('keydown', 'keypress', 'keyup')]
    browser.evaluate("for(const kind of ['keydown','keypress','keyup'])document.removeEventListener(kind,sourceKeyListener,true)")
    replace_source(browser, text.replace('Continue', 'Edited live'))
    compile_ready(browser)
    browser.until("Module.previewTree?.some(n => n.label === 'Edited live')", 'edited label in guest widget tree')
    edited = browser.evaluate("Module.previewTree.find(n => n.label === 'Edited live' && n.interactive)")
    mouse(browser, edited)
    browser.until("Module.previewTree?.some(n => n.key === 2 && n.label === '1')", 'edited guest Button interaction')
    capture(browser, shots / 'edited-source.webp')
    replace_source(browser, 'export function Frame(): void {\n    const broken = ;\n}\n')
    browser.until('!Module.compileBusy && Module.compileDiagnostic?.length > 0', 'compiler diagnostics')
    assert browser.evaluate('Module.previewReady'), 'last valid preview discarded'
    replace_source(browser, text)
    compile_ready(browser)
    assert not browser.errors, browser.errors


def check_visual_polish(browser, base, scenes, shots):
    for width in (320, 390, 1366):
        for dark in (False, True):
            ready(browser, base, 'getting-started', width=width)
            if browser.evaluate('Module.siteDark') != dark:
                mouse(browser, node_for(browser, key=10003, width=width), width=width)
                browser.until('Module.siteDark === ' + str(dark).lower(), 'guide theme')
            assert not any('ZIRAN UI' in n['label'] for n in tree(browser))
            for key in (10613, 10618, 10624):
                node = next(n for n in tree(browser) if n['key'] == key)
                assert node['x'] >= 16 and node['x'] + node['width'] <= width - 16, node
            capture(browser, shots / ('guide-%s-%s-top.webp' % (width, dark)))
            browser.command('Input.dispatchMouseEvent', type='mouseMoved', x=width-20, y=700)
            initial_y = next(n for n in tree(browser) if n['key'] == 10622)['y']
            for step, target in enumerate((10616, 10622, 10052)):
                # The final 42px link rests above the bottom padding at max scroll.
                limit = 782 if target == 10052 else 600
                for attempt in range(12):
                    if next(n for n in tree(browser) if n['key'] == target)['y'] < limit:
                        break
                    browser.command('Input.dispatchMouseEvent', type='mouseWheel', x=width-20, y=700, deltaX=0, deltaY=470)
                    time.sleep(.25)
                browser.until('Module.siteTree.some(n=>n.key===' + str(target) + ' && n.y<' + str(limit) + ')', 'Guide section scrolls into view')
                assert next(n for n in tree(browser) if n['key'] == 10622)['y'] < initial_y - 100
                if target == 10052:
                    link = next(n for n in tree(browser) if n['key'] == target)
                    assert link['y'] >= 104 and link['y'] + link['height'] <= 824, link
                capture(browser, shots / ('guide-%s-%s-%s.webp' % (width, dark, step)))
            if width == 1366:
                ready(browser, base, 'getting-started')
                mouse(browser, node_for(browser, key=10603))
                browser.until('Module.siteTree.some(n=>n.key===10622 && n.y<250)', 'guide theme contents link')
    ready(browser, base, 'index')
    if browser.evaluate('Module.siteDark'):
        mouse(browser, node_for(browser, key=10003))
    assert not any('ZIRAN UI' in n['label'] for n in tree(browser))
    capture(browser, shots/'hold-idle-after.webp')
    browser.screenshot(shots/'hold-idle-after.png')
    n = node_for(browser, key=10043)
    mouse(browser, n, click=False)
    x, y = point(n, 1366)
    browser.command('Input.dispatchMouseEvent',type='mousePressed',x=x,y=y,button='left',clickCount=1)
    browser.until('Module.siteHoldProgress > .35 && Module.siteHoldProgress < .9', 'intermediate hold fill')
    capture(browser, shots/'hold-held-after.webp')
    browser.screenshot(shots/'hold-held-after.png')
    browser.until('Module.siteHoldCount===1', 'hold completes')
    browser.command('Input.dispatchMouseEvent',type='mouseReleased',x=x,y=y,button='left',clickCount=1)
    mouse(browser, node_for(browser,key=10044))
    browser.until("Module.siteTree.some(n=>n.key===10044 && n.label==='Confirm delete')", 'confirm expanded')
    time.sleep(.35)
    capture(browser,shots/'confirm-expanded-after.webp')
    browser.screenshot(shots/'confirm-expanded-after.png')
    mouse(browser,node_for(browser,key=10044))
    browser.until('Module.siteConfirmCount===1','confirm commits once')
    select_example(browser,base,scenes,'Image')
    check_picture(browser,'Image',shots)
    assert any(n['label']=='Alpine morning' for n in tree(browser))
    mouse(browser,node_for(browser,label='Source'))
    compile_ready(browser)
    assert browser.evaluate('Module.previewReady'), 'Image portable source failed'
    capture(browser,shots/'image-editable-after.webp')
    print('Local polish browser tests: Guide responsive/light/dark, native Image/source, Hold progress/completion and confirmation passed')


def check_followup(browser, base, scenes, shots):
    groups = {(s['moduleId'], s['category']) for s in scenes}
    assert ('plot', 'Plotting') in groups and ('data_views', 'Data views') in groups
    assert len({s['id'] for s in scenes}) == 101
    for portable in (False, True):
        select_example(browser, base, scenes, 'Onboarding checklist')
        if portable:
            mouse(browser, node_for(browser, label='Source')); compile_ready(browser)
        collection = 'Module.previewTree' if portable else 'Module.siteTree'
        for dark in (False, True):
            if browser.evaluate('Module.siteDark') != dark: mouse(browser, node_for(browser, key=10003))
            frame = browser.evaluate('Module.siteFrame')
            browser.until('Module.siteFrame > ' + str(frame + 4), 'checklist theme rendered')
            heading = browser.evaluate(collection + ".find(n=>n.label==='Get started')")
            nodes = browser.evaluate(collection)
            card = next(n for n in nodes if n['key'] == 10 and not n['interactive'] and
                        n['x'] <= heading['x'] < n['x'] + n['width'] and
                        n['y'] <= heading['y'] < n['y'] + n['height'])
            image = frame_crop(browser, card)
            surface = image.getpixel((5, 5))
            assert (sum(surface) < 200) == dark, ('checklist surface does not follow palette', surface, dark)
            for label in ('Get started', 'Create a project', 'Add a component', 'Run the app'):
                n = browser.evaluate(collection + '.find(n=>n.label===' + json.dumps(label) + ' && !n.interactive)')
                assert_text_contrast(frame_crop(browser, n), label + str(dark))
            capture(browser, shots / ('checklist-%s-%s.webp' % (portable, dark)))
        scope = collection + '.filter(n=>n.x>=' + str(card['x']) + ' && n.x<' + str(card['x'] + card['width']) + ' && n.y>=' + str(card['y']) + ' && n.y<' + str(card['y'] + card['height']) + ')'
        circle = browser.evaluate(scope + '.find(n=>n.key===11 && n.interactive)')
        before = frame_crop(browser, card)
        mouse(browser, circle)
        browser.until(collection + ".some(n=>n.label==='1 of 3 complete')", 'checklist task toggled and progress updated')
        after = frame_crop(browser, card)
        assert ImageChops.difference(before, after).getbbox(), 'checklist completion did not paint'
        capture(browser, shots / ('checklist-checked-%s.webp' % portable))
        mouse(browser, node_for(browser, key=9004))
        browser.until(scope + '.some(n=>n.key===11 && n.disabled)', 'checklist disabled row')
        mouse(browser, browser.evaluate(scope + '.find(n=>n.key===11 && n.interactive)'))
        assert browser.evaluate(collection + ".some(n=>n.label==='1 of 3 complete')"), 'disabled checklist changed'
        capture(browser, shots / ('checklist-disabled-%s.webp' % portable))
    select_example(browser, base, scenes, 'Carousel')
    check_picture(browser, 'Carousel', shots)
    first = frame_crop(browser, node_for(browser, key=10101))
    mouse(browser, node_for(browser, label='Next'))
    browser.until("Module.siteTree.some(n=>n.label==='Follow the trail along the ocean')", 'second bitmap slide')
    time.sleep(.7)
    check_picture(browser, 'Carousel coast', shots)
    assert ImageChops.difference(first, frame_crop(browser, node_for(browser, key=10101))).getbbox(), 'Carousel photo did not change'
    for dark in (False, True):
        if browser.evaluate('Module.siteDark') != dark: mouse(browser, node_for(browser, key=10003))
        caption = node_for(browser, label='Follow the trail along the ocean')
        pixels = frame_crop(browser, caption)
        w, h = pixels.size
        assert_text_contrast(pixels.crop((h // 2, h // 4, w - h // 2, h * 3 // 4)), 'Carousel caption ' + str(dark))
        capture(browser, shots / ('carousel-caption-%s.webp' % dark))
    mouse(browser, node_for(browser, label='Source')); compile_ready(browser)
    for dark in (False, True):
        if browser.evaluate('Module.siteDark') != dark: mouse(browser, node_for(browser, key=10003))
        caption = browser.evaluate("Module.previewTree.find(n=>n.label==='A quiet morning in the mountains')")
        assert caption, 'Portable Carousel caption missing'
        pixels = frame_crop(browser, caption)
        w, h = pixels.size
        assert_text_contrast(pixels.crop((h // 2, h // 4, w - h // 2, h * 3 // 4)), 'Portable Carousel caption ' + str(dark))
        capture(browser, shots / ('carousel-source-caption-%s.webp' % dark))
    capture(browser, shots/'carousel-source.webp')
    print('Local followup tests: grouped modules, native/portable checklist light/dark contrast, checked/disabled progress, two real Carousel images/source passed')


def main():
    from site_browser_timeout_test import main as check_timeout_guards
    check_timeout_guards()
    from widget_examples_source_test import main as check_source_preservation
    run_step('maintained example sources', check_source_preservation, seconds=600)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--followup-only', action='store_true')
    parser.add_argument('--polish-only', action='store_true')
    parser.add_argument('--compiler-only', action='store_true')
    parser.add_argument('--native-tsx-only', action='store_true')
    parser.add_argument('--options-only', action='store_true')
    parser.add_argument('--routes-only', action='store_true')
    parser.add_argument('--compiler-scene', action='append', default=[], help='Compile only the named source variants')
    parser.add_argument('--site', type=Path, default=SITE)
    parser.add_argument('--shots', type=Path, default=ROOT / 'build/scratch/site-browser')
    args = parser.parse_args()
    if args.native_tsx_only:
        scenes = []
    elif args.compiler_only:
        text = (args.site / 'gallery-data.js').read_text()
        scenes = json.loads(text.split('window.galleryScenes = ', 1)[1].rstrip(';\n'))
    else: scenes = check_files(args.site)
    if args.compiler_scene:
        assert args.compiler_only, '--compiler-scene requires --compiler-only'
        scenes = [scene for scene in scenes if scene['name'] in args.compiler_scene]
        assert len(scenes) == len(set(args.compiler_scene)), 'Unknown compiler scene'
    chromium = (shutil.which(os.environ['CHROMIUM']) if os.environ.get('CHROMIUM') else
                shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome'))
    assert chromium, 'Chromium or Chrome is required for the integrated browser test'
    args.shots.mkdir(parents=True, exist_ok=True)
    server = serve(args.site)
    browser = run_step('browser startup', Browser, args.shots / 'profile', chromium, seconds=60)
    try:
        base = 'http://127.0.0.1:' + str(server.server_port) + '/'
        if args.native_tsx_only:
            run_step('native TSX browser compiler', check_native_tsx_compiler, browser, base, seconds=300)
            return
        if args.followup_only:
            run_step('followup examples', check_followup, browser, base, scenes, args.shots, seconds=600)
            return
        if args.polish_only:
            run_step('visual polish', check_visual_polish, browser, base, scenes, args.shots, seconds=600)
            return
        if args.compiler_only:
            run_step('browser compiler', check_compiler, browser, base, scenes, seconds=1200)
            return
        if args.routes_only:
            run_step('current routes', check_clean_routes, browser, base)
            return
        if not args.options_only:
            run_step('current routes', check_clean_routes, browser, base)
            run_step('document layout', check_document_layout, browser, base, args.shots, seconds=300)
            run_step('preview containment', check_preview_containment, browser, base, args.shots, seconds=300)
            run_step('scroll dragging', check_scroll_drag, browser, base)
            run_step('application cards', check_apps, browser, base, args.shots)
            # GitHub runners compile each of the 101 sources in about 14s, so
            # this phase takes about 24 minutes there (under 15 locally).
            # compile_ready still bounds each compile to 120s.
            run_step('live examples and source editing', check_live, browser, base, scenes, args.shots, seconds=2400)
        run_step('example options', check_example_options, browser, base, scenes, args.shots, seconds=600)
        print('Local website options tests: disabled flags, motion and contrast passed' if args.options_only else 'Local website browser tests: routes, 101 live scenes/sources, mobile, theme, editing, diagnostics, portable preview, disabled flags, motion and contrast passed')
    except Exception:
        print('Browser runtime errors:', browser.errors[-6:])
        print('Browser console diagnostics:', browser.console_messages)
        try:
            with test_step('failure diagnostics', 10):
                print('Browser failure state:', browser.evaluate('window.compilerReply') if args.compiler_only or args.native_tsx_only else browser.evaluate("({scene:Module.gallerySceneRendered,key:Module.lastInputKey,pickerKey:Module.lastPickerKey,active:document.activeElement?.id,focused:Module.siteTree?.filter(n=>n.focused).map(n=>({key:n.key,parent:n.parent,label:n.label})),errors:document.querySelector('#result')?.textContent,compileBusy:Module.compileBusy,compileRevision:Module.compileRevision,editorRevision:Module.editorRevision,compilerResult:Module.compilerResult,diagnostic:Module.compileDiagnostic})"))
                capture(browser, args.shots / 'failure.webp')
        except Exception as diagnostic:
            print('Browser diagnostics unavailable:', diagnostic, flush=True)

        raise
    finally:
        browser.close(); server.shutdown(); server.server_close()


if __name__ == '__main__': main()
