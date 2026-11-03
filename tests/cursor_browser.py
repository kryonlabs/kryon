"""Web cursors of the cursor probe in an owned headless Chromium (Canvas or DOM).

  python3 tests/cursor_browser.py PAGE.html PROFILE-DIR

Drives only the private browser started here over its DevTools pipe, with no
desktop connection. At device pixel ratio 2 the element under the pointer must
show each request as one CSS cursor: a 2x density image set, drawn at the
logical size, with the requested hotspot; the application cursor after a
withdraw; none when hidden. A request repeated every frame must never touch
any element's cursor style.
"""
import base64
import io
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import time

from PIL import Image

POINTS = ((320, 120), (60, 40), (580, 200), (300, 30), (40, 210))
WATCH = '''(() => {
  globalThis.cursorProbe = {changes: [], last: new WeakMap()};
  const watch = element => {
    const value = element.style ? element.style.cursor : '';
    const before = cursorProbe.last.get(element);
    if (before !== undefined && before !== value) cursorProbe.changes.push([element.id || element.tagName, before.slice(0, 40), value.slice(0, 40)]);
    cursorProbe.last.set(element, value);
  };
  document.querySelectorAll('*').forEach(watch);
  new MutationObserver(list => list.forEach(change => change.target.nodeType === 1 && watch(change.target)))
    .observe(document.documentElement, {subtree: true, childList: true, attributes: true, attributeFilter: ['style']});
})()'''


class Browser:
    def __init__(self, profile):
        shutil.rmtree(profile, ignore_errors=True)
        profile.mkdir(parents=True)
        environment = dict(os.environ)
        for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS',
                     'SESSION_MANAGER'):
            environment.pop(name, None)
        environment['YUE_DESKTOP_RECOVERY'] = '0'
        # Chromium's temporary Unix socket must stay below the 108-byte limit.
        environment['TMPDIR'] = str(profile)
        self.log = (profile / 'chromium.log').open('wb')
        self.process = subprocess.Popen(
            ['sh', '-c', 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', 'chromium',
             os.environ.get('CHROMIUM', 'chromium'), '--headless', '--no-sandbox',
             '--disable-gpu', '--disable-webgl', '--disable-software-rasterizer',
             '--disable-gpu-compositing', '--use-gl=disabled', '--disable-dev-shm-usage',
             '--no-first-run', '--no-default-browser-check', '--allow-file-access-from-files',
             '--remote-debugging-pipe', '--user-data-dir=' + str(profile), 'about:blank'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, env=environment)
        self.received = b''
        self.sequence = 0
        self.session = None
        self.errors = []

    def command(self, method, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session:
            message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                if reply.get('method') == 'Runtime.exceptionThrown':
                    self.errors.append(str(reply['params'])[:1000])
                if reply.get('id') == self.sequence:
                    if 'error' in reply:
                        raise RuntimeError(method + ': ' + str(reply['error']))
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data:
                    raise RuntimeError('Chromium exited (%s)' % self.process.wait(timeout=2))
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression):
        result = self.command('Runtime.evaluate', expression=expression, returnByValue=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:1000])
        return result.get('result', {}).get('value')

    def until(self, expression, what, seconds=20):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            assert not self.errors, (what, self.errors)
            value = self.evaluate(expression)
            if value:
                return value
            time.sleep(.05)
        raise AssertionError(what + ': timed out')

    def open(self, page):
        target = next(item['targetId'] for item in self.command('Target.getTargets')['targetInfos']
                      if item['type'] == 'page')
        self.session = self.command('Target.attachToTarget', targetId=target, flatten=True)['sessionId']
        self.command('Runtime.enable')
        self.command('Page.enable')
        self.command('Emulation.setDeviceMetricsOverride', width=640, height=240, deviceScaleFactor=2, mobile=False)
        self.command('Page.navigate', url=page.as_uri())

    def move(self, x, y):
        self.command('Input.dispatchMouseEvent', type='mouseMoved', x=x, y=y)

    def digit(self, number):
        key = str(number)
        for kind in ('keyDown', 'keyUp'):
            self.command('Input.dispatchKeyEvent', type=kind, key=key, code='Digit' + key,
                         windowsVirtualKeyCode=48 + number, nativeVirtualKeyCode=48 + number,
                         text=key if kind == 'keyDown' else '')

    def under(self, x, y):
        return self.evaluate(f'(() => {{ const e = document.elementFromPoint({x}, {y}); '
                             f'return e ? getComputedStyle(e).cursor : "" }})()')

    def close(self):
        try:
            self.command('Browser.close')
        except (BrokenPipeError, RuntimeError, TimeoutError):
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.log.close()


def images(css):
    found = re.findall(r'url\("?data:image/png;base64,([A-Za-z0-9+/=]+)"?\)', css)
    assert found, 'cursor has no bitmap: ' + css[:120]
    sizes = []
    for data in found:
        with Image.open(io.BytesIO(base64.b64decode(data))) as picture:
            assert picture.convert('RGBA').getchannel('A').getextrema()[1] == 255, 'cursor bitmap is empty'
            sizes.append(picture.size)
    return sorted(set(sizes))


def expect(browser, what, hotspot=None, size=None, hidden=False):
    """Every point shows one cursor for WHAT; returns its CSS."""
    x, y = POINTS[0]
    browser.move(x, y)
    if hidden:
        test = 'c === "none"'
    else:
        test = f'c.includes("url(") && / {hotspot[0]} {hotspot[1]}, /.test(c)'
    css = browser.until(f'(() => {{ const e = document.elementFromPoint({x}, {y}); '
                        f'const c = e ? getComputedStyle(e).cursor : ""; return {test} ? c : "" }})()', what)
    if not hidden:
        width, height = size
        # The 2x density image, shown at the logical size.
        assert images(css) == [(width * 2, height * 2)], (what, images(css))
        assert 'image-set(' in css and re.search(r'" ?\) 2(x|dppx)\)', css), (what, 'no density image', css[-80:])
    for x, y in POINTS[1:]:
        browser.move(x, y)
        assert browser.under(x, y) == css, (what, 'a different cursor under the pointer', x, y)
    return css


def main():
    page, profile = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    browser = Browser(profile)
    try:
        browser.open(page)
        browser.until('document.readyState === "complete" && !!document.elementFromPoint(320, 120)', 'page ready', 60)
        browser.evaluate('document.querySelector("canvas") && document.querySelector("canvas").focus()')
        # Mode 1 asks for the hand every frame: one CSS cursor, never rewritten.
        hand = expect(browser, 'hand', (3, 5), (32, 24))
        browser.evaluate(WATCH)
        time.sleep(1)
        for x, y in POINTS * 2:
            browser.move(x, y)
            time.sleep(.05)
        changes = browser.evaluate('cursorProbe.changes')
        assert not changes, ('a repeated request rewrote the cursor style', changes)
        browser.digit(3)
        expect(browser, 'application cursor after withdraw', (7, 1), (32, 32))
        browser.digit(2)
        expect(browser, 'hidden', hidden=True)
        browser.digit(8)
        expect(browser, 'tool', (4, 20), (32, 24))
        browser.digit(5)
        expect(browser, 'moved hotspot', (9, 5), (32, 24))
        browser.digit(6)
        time.sleep(.5)
        assert expect(browser, 'refused size keeps the request', (9, 5), (32, 24))
        browser.digit(1)
        assert expect(browser, 'hand again', (3, 5), (32, 24)) == hand, 'the hand cursor changed'
        assert not browser.errors, browser.errors
    finally:
        browser.close()
    print(f'{page.name}: hand, application, hidden, tool and hotspot cursors as 2x image sets, '
          'steady under repeated requests', flush=True)


if __name__ == '__main__':
    main()
