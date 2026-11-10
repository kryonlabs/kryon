#!/usr/bin/env python3
"""Exercise the compiled Ziran website in a private Chromium session."""
import argparse
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
REDIRECT_TARGETS = {
    'api': 'docs.html',
    'components': 'widgets.html', 'demo': 'widgets.html', 'live-examples': 'widgets.html',
    'language': 'docs.html', 'showcase': 'apps.html', 'matrices': 'renderers.html',
    'benchmarks': 'docs.html',
}
REDIRECTS = list(REDIRECT_TARGETS)
VIEWPORTS = (320, 390, 768, 1024, 1366)
EXPECTED_SCENE_COUNT = 101


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
        environment.update(YUE_DESKTOP_RECOVERY='0')
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
        self.received, self.sequence, self.session, self.errors = b'', 0, None, []
        target = next(item for item in self.command('Target.getTargets')['targetInfos'] if item['type'] == 'page')
        self.session = self.command('Target.attachToTarget', targetId=target['targetId'], flatten=True)['sessionId']
        for domain in ('Runtime', 'Page', 'Log'):
            self.command(domain + '.enable')

    def close(self):
        try: self.process.stdin.close()
        except OSError: pass
        try: os.killpg(self.process.pid, 15)
        except ProcessLookupError: pass
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(self.process.pid, 9); self.process.wait()
        self.log.close()
        if self.runtime: self.runtime.cleanup()

    def command(self, method, timeout=30, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session: message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw: continue
                reply = json.loads(raw)
                event = reply.get('method')
                if event == 'Runtime.exceptionThrown':
                    details = reply['params']['exceptionDetails']
                    self.errors.append((details.get('exception') or {}).get('description') or details.get('text'))
                if reply.get('id') == self.sequence:
                    if 'error' in reply: raise DevToolsError(method, reply['error'])
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data: raise RuntimeError('Chromium exited')
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression):
        result = self.command('Runtime.evaluate', expression=expression,
                              returnByValue=True, awaitPromise=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:1200])
        return result.get('result', {}).get('value')

    def until(self, expression, what, seconds=30):
        deadline = time.monotonic() + seconds
        value = None
        while time.monotonic() < deadline:
            try: value = self.evaluate(expression)
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
        self.until("document.readyState === 'complete'", 'load ' + url)
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
    for name in PAGES:
        source = (site / (name + '.html')).read_text()
        assert source.count('<canvas ') == 1 and '<iframe' not in source
        assert 'current.js' in source and 'id="kryon-ime"' in source
        assert '<button' not in source and '<select' not in source, 'Website controls must be Ziran'
    assert (site / 'runtime/simd128.js').stat().st_size > 10000
    assert (site / 'runtime/baseline.js').stat().st_size > 10000
    assets = dict(json.loads((site / 'runtime/assets.json').read_text()))
    image = site / assets['/assets/pfp.webp']
    assert hashlib.sha256(image.read_bytes()).digest() == hashlib.sha256((ROOT / 'examples/gallery/assets/pfp.webp').read_bytes()).digest(), 'Gallery image asset missing or changed'
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


def check_compiler(browser, base, scenes):
    browser.open(base + 'compiler-worker.js')
    browser.evaluate("""window.compilerWorker = new Worker('compiler-worker.js');
      window.compilerReply = null;
      compilerWorker.onmessage = ({data}) => { window.compilerReply = {id:data.id, status:data.status,
        diagnostics:data.diagnostics, bytes:data.bytes?.byteLength || 0}; };
      compilerWorker.onerror = e => { window.compilerReply = {status:1, diagnostics:e.message}; };""")
    search = browser.evaluate("fetch('gallery-data.js').then(r => r.text()).then(t => { (0,eval)(t); return window.galleryScenes.length; })")
    assert search == EXPECTED_SCENE_COUNT
    sys.path.insert(0, str(ROOT / 'scripts'))
    from gallery import compiler_paths, enabled_modules
    search = '\n'.join(compiler_paths(enabled_modules()))
    for index, scene in enumerate(scenes):
        browser.evaluate('fetch(' + json.dumps(scene['code']) + ').then(r => r.text()).then(source => { window.compilerReply = null; compilerWorker.postMessage({id:' + str(index + 1) + ',source,search:' + json.dumps(search) + '}); })')
        reply = browser.until('window.compilerReply', 'source compile ' + scene['name'], 120)
        assert reply['status'] == 0 and reply['bytes'] > 0, (scene['name'], reply)
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


def select_example(browser, base, scenes, name, reload=True):
    if reload: ready(browser, base)
    nodes = tree(browser)
    nav = next(n for n in nodes if n['key'] == 9003)
    selected = next(n for n in nodes if n['selected'] and n['parent'] == nodes.index(nav))
    mouse(browser, selected)
    browser.key('Home', 36)
    index = next(i for i, scene in enumerate(scenes) if scene['name'] == name)
    if index > len(scenes) // 2:
        browser.key('End', 35)
        for _ in range(len(scenes) - 1 - index): browser.key('ArrowUp', 38)
    else:
        for _ in range(index): browser.key('ArrowDown', 40)
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
        overflow = ('using UI :: #import "kryon/Widgets";\n#program_export\n'
                    'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
                    ' Box(session, BoxProps.{.key = 771, .bounds = .{viewport.x - 80.0, viewport.y - 80.0, '
                    'viewport.width + 160.0, viewport.height + 160.0}, .fill = .{247, 0, 223, 255}});\n return 0;\n}\n'
                    '#program_export\nSetExampleOptions :: (disabled: bool, reduced_motion: bool) { unused disabled; unused reduced_motion }\n')
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
    capture(browser, shots/'overview-phone.webp')
    mouse(browser, node_for(browser, key=10520, width=390), width=390)
    mouse(browser, node_for(browser, key=10524, width=390), width=390)
    browser.until('Module.sitePage===5', 'phone resources route')
    ready(browser, base, 'widgets', width=390)
    mouse(browser, node_for(browser, label='Source', width=390), width=390)
    compile_ready(browser)
    mouse(browser, node_for(browser, key=10103, width=390), width=390)
    browser.until('Module.siteTree.some(n => n.key===9003)', 'mobile source navigation')
    mouse(browser, node_for(browser, key=2, width=390), width=390)
    browser.until('Module.gallerySceneRendered===1', 'mobile source changes real scene')
    compile_ready(browser)
    capture(browser, shots/'documentation-phone-source.webp')
    header = frame_crop(browser, dict(x=0,y=0,width=390,height=104))
    browser.command('Input.dispatchMouseEvent',type='mouseWheel',x=382,y=620,deltaX=0,deltaY=720)
    browser.until('Module.siteTree.some(n => n.key===10100 && n.y<0)', 'phone article scroll')
    time.sleep(.3)
    assert ImageChops.difference(header, frame_crop(browser, dict(x=0,y=0,width=390,height=104))).getbbox() is None, 'Guest preview painted over navigation'
    capture(browser, shots/'documentation-phone-scrolled.webp')



def check_live(browser, base, scenes, shots):
    ready(browser, base, 'index')
    mouse(browser, node_for(browser, key=10038))
    browser.until('Module.siteCount === 1', 'real counter click')
    mouse(browser, node_for(browser, key=10003))
    browser.until('Module.siteDark', 'theme toggle')
    browser.command('Page.reload')
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
    first = node_for(browser, label=scenes[0]['name'], key=1, parent_key=9003)
    mouse(browser, first)
    for index, scene in enumerate(scenes):
        if index: browser.key('ArrowDown', 40)
        browser.until('Module.gallerySceneRendered === ' + str(index), 'sidebar ' + scene['name'])
        browser.until('Module.siteTree?.some(n => n.key === 10100 && n.label === ' + json.dumps(scene['name']) + ')', 'committed title ' + scene['name'])
        if scene['name'] in ('Image', 'Carousel', 'Tile map'): check_picture(browser, scene['name'], shots)
    capture(browser, shots / 'components.webp')
    for width in VIEWPORTS:
        ready(browser, base, width=width)
        for key in (10002, 10003, 10012, 10104):
            assert visible(node_for(browser, key=key, width=width), width), (width, key)
        if width < 760:
            mouse(browser, node_for(browser, key=10103, width=width), width)
            mouse(browser, node_for(browser, key=2, label=scenes[1]['name'], width=width, parent_key=9003), width)
            browser.until('Module.gallerySceneRendered === 1', 'mobile picker')
        capture(browser, shots / ('components-' + str(width) + '.webp'))
    ready(browser, base)
    mouse(browser, node_for(browser, label='Source'))
    for index, scene in enumerate(scenes):
        if index:
            nav = node_for(browser, key=9003)
            selected = next(n for n in tree(browser) if n['selected'] and n['parent'] == tree(browser).index(nav))
            mouse(browser, selected)
            browser.key('ArrowDown', 40)
            browser.until('Module.gallerySceneRendered === ' + str(index), 'source selection')
        compile_ready(browser)
    ready(browser, base)
    mouse(browser, node_for(browser, label='Source'))
    compile_ready(browser)
    text = node_for(browser, key=10102)['value']
    assert 'Button(' in text and 'Continue' in text
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
    replace_source(browser, 'this is invalid Ziran')
    browser.until('!Module.compileBusy && Module.compileDiagnostic?.length > 0', 'compiler diagnostics')
    assert browser.evaluate('Module.previewReady'), 'last valid preview discarded'
    replace_source(browser, text)
    compile_ready(browser)
    assert not browser.errors, browser.errors


def main():
    from widget_examples_source_test import main as check_source_preservation
    check_source_preservation()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-only', action='store_true')
    parser.add_argument('--options-only', action='store_true')
    parser.add_argument('--compiler-scene', action='append', default=[], help='Compile only the named source variants')
    parser.add_argument('--site', type=Path, default=SITE)
    parser.add_argument('--shots', type=Path, default=ROOT / 'build/scratch/site-browser')
    args = parser.parse_args()
    if args.compiler_only:
        text = (args.site / 'gallery-data.js').read_text()
        scenes = json.loads(text.split('window.galleryScenes = ', 1)[1].rstrip(';\n'))
    else: scenes = check_files(args.site)
    if args.compiler_scene:
        assert args.compiler_only, '--compiler-scene requires --compiler-only'
        scenes = [scene for scene in scenes if scene['name'] in args.compiler_scene]
        assert len(scenes) == len(set(args.compiler_scene)), 'Unknown compiler scene'
    chromium = shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome')
    assert chromium, 'Chromium or Chrome is required for the integrated browser test'
    args.shots.mkdir(parents=True, exist_ok=True)
    server = serve(args.site)
    browser = Browser(args.shots / 'profile', chromium)
    try:
        base = 'http://127.0.0.1:' + str(server.server_port) + '/'
        if args.compiler_only:
            check_compiler(browser, base, scenes)
            return
        if not args.options_only:
            check_document_layout(browser, base, args.shots)
            check_preview_containment(browser, base, args.shots)
            check_live(browser, base, scenes, args.shots)
        check_example_options(browser, base, scenes, args.shots)
        print('Local website options tests: disabled flags, motion and contrast passed' if args.options_only else 'Local website browser tests: routes, 101 live scenes/sources, mobile, theme, editing, diagnostics, portable preview, disabled flags, motion and contrast passed')
    except Exception:
        print('Browser failure state:', browser.evaluate('window.compilerReply') if args.compiler_only else browser.evaluate("({scene:Module.gallerySceneRendered,key:Module.lastInputKey,pickerKey:Module.lastPickerKey,active:document.activeElement?.id,focused:Module.siteTree?.filter(n=>n.focused).map(n=>({key:n.key,parent:n.parent,label:n.label})),errors:document.querySelector('#result')?.textContent,compileBusy:Module.compileBusy,compileRevision:Module.compileRevision,editorRevision:Module.editorRevision,compilerResult:Module.compilerResult,diagnostic:Module.compileDiagnostic})"))
        capture(browser, args.shots / 'failure.webp')
        raise
    finally:
        browser.close(); server.shutdown(); server.server_close()


if __name__ == '__main__': main()
