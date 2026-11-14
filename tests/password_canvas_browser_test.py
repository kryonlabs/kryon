#!/usr/bin/env python3
"""Secure widget through real keyboard events in an owned headless Chromium.

Uses the repository's DevTools pipe convention, without Node or a desktop.
Run through build-queue; all generated files and the browser profile are ignored.
"""
import fcntl
import json
import os
from pathlib import Path
import select
import shlex
import shutil
import subprocess
import time

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/pw-canvas'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH', 'KRYON_TEST_DRIVER'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV['EMCC_CORES'] = '1'
ENV['EM_CACHE'] = str(ROOT / 'build/emscripten-cache')
# Keep Chromium's temporary Unix socket below Linux's 108-byte path limit.
ENV['TMPDIR'] = str(WORK)


def run(args):
    print(shlex.join(map(str, args[:8])) + (' ...' if len(args) > 8 else ''), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)


class Browser:
    def __init__(self):
        profile = WORK / 'profile'
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
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, env=ENV)
        self.received = b''
        self.sequence = 0
        self.session = None
        self.diagnostics = []

    def open(self, html):
        target = next(item['targetId'] for item in self.command('Target.getTargets')['targetInfos']
                      if item['type'] == 'page')
        self.session = self.command('Target.attachToTarget', targetId=target, flatten=True)['sessionId']
        self.command('Runtime.enable')
        self.command('Page.enable')
        self.command('Emulation.setDeviceMetricsOverride', width=640, height=240,
                     deviceScaleFactor=1, mobile=False)
        self.command('Page.addScriptToEvaluateOnNewDocument', source='''
            globalThis.passwordEvents = [];
            for (const type of ['keydown', 'keyup'])
                addEventListener(type, event => passwordEvents.push({type,
                    key: event.key, ctrl: event.ctrlKey, trusted: event.isTrusted}), true);
        ''')
        self.command('Page.navigate', url=html.as_uri())

    def command(self, method, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session:
            message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                if reply.get('method') == 'Runtime.exceptionThrown':
                    self.diagnostics.append(str(reply['params'])[:2000])
                if reply.get('id') == self.sequence:
                    if 'error' in reply:
                        raise RuntimeError(method + ': ' + str(reply['error']))
                    return reply.get('result', {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 65536)
                if not data:
                    code = self.process.wait(timeout=2)
                    raise RuntimeError('Chromium exited (' + str(code) + '); see ' + str(WORK / 'chromium.log'))
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression):
        result = self.command('Runtime.evaluate', expression=expression, returnByValue=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:2000])
        return result.get('result', {}).get('value')

    def state(self):
        return self.evaluate('''({frames: globalThis.passwordFrames, value: globalThis.passwordValue,
            cursor: globalThis.passwordCursor, anchor: globalThis.passwordAnchor,
            focused: globalThis.passwordFocused, edits: globalThis.passwordEdits,
            key: globalThis.passwordKey, modifiers: globalThis.passwordModifiers,
            lastKey: globalThis.passwordLastKey, lastModifiers: globalThis.passwordLastModifiers,
            paused: globalThis.passwordPaused,
            heldModifiers: globalThis.passwordHeldModifiers,
            snapshotModifiers: globalThis.passwordSnapshotModifiers,
            snapshotControl: globalThis.passwordSnapshotControl, shortcuts: globalThis.passwordShortcuts,
            start: globalThis.passwordEditStart, end: globalThis.passwordEditEnd,
            selectedPaint: globalThis.passwordSelectionPainted, failure: globalThis.passwordFailure})''')

    def until(self, predicate, what):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            state = self.state()
            if state.get('failure') or self.diagnostics:
                raise AssertionError(what + ': ' + json.dumps(state) + ' ' + str(self.diagnostics))
            if predicate(state):
                return state
            time.sleep(.02)
        raise AssertionError(what + ': timed out ' + json.dumps(self.state()))

    def frames(self, count=4):
        start = self.state()['frames']
        return self.until(lambda s: s.get('frames', 0) >= start + count, 'idle frames')

    def key(self, key, code, vk, down, modifiers=0, text='', repeat=False):
        self.command('Input.dispatchKeyEvent', type='keyDown' if down else 'keyUp',
                     key=key, code=code, windowsVirtualKeyCode=vk, nativeVirtualKeyCode=vk,
                     modifiers=modifiers, text=text if down else '', autoRepeat=repeat)

    def tap(self, key, code, vk, text=''):
        self.key(key, code, vk, True, text=text)
        self.key(key, code, vk, False)
        return self.frames()

    def select_all(self):
        before = self.state()
        self.key('Control', 'ControlLeft', 17, True, modifiers=2)
        self.frames(2)
        self.key('a', 'KeyA', 65, True, modifiers=2)
        selected = self.until(lambda s: s.get('anchor') == 0 and s.get('cursor') ==
                              len(before['value'].encode()) and s.get('selectedPaint') == 1,
                              'Ctrl+A retained selection and painted highlight')
        assert selected['value'] == before['value'] and selected['edits'] == before['edits'], selected
        self.key('a', 'KeyA', 65, False, modifiers=2)
        self.key('Control', 'ControlLeft', 17, False)
        after = self.frames()
        assert after['cursor'] == selected['cursor'] and after['anchor'] == 0, after
        return after

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


def exercise(html):
    browser = Browser()
    try:
        browser.open(html)
        browser.until(lambda s: s.get('frames', 0) > 3, 'Canvas ready')
        # DOM focus only: no owner display, browser or pointer is used.
        browser.evaluate('document.getElementById("canvas").focus()')
        assert browser.state()['value'] == 'Aé🔑xyz'
        first = browser.tap('Backspace', 'Backspace', 8)
        assert first['value'] == 'Aé🔑xy' and first['cursor'] == first['anchor'] == 9, first
        print('PASS ordinary Backspace removes one character, retains nonempty password', flush=True)
        browser.tap('z', 'KeyZ', 90, text='z')
        selected = browser.select_all()
        assert selected['cursor'] == 10 and selected['value'] == 'Aé🔑xyz', selected
        replaced = browser.tap('q', 'KeyQ', 81, text='q')
        assert replaced['value'] == 'q' and replaced['cursor'] == replaced['anchor'] == 1, replaced
        assert replaced['start'] == 0 and replaced['end'] == 10, replaced
        print('PASS Ctrl+A selects full UTF-8 byte range across idle frames; typing replaces all', flush=True)
        two = browser.tap('r', 'KeyR', 82, text='r')
        assert two['value'] == 'qr', two
        one = browser.tap('Backspace', 'Backspace', 8)
        assert one['value'] == 'q' and one['start'] == 1 and one['end'] == 2, one
        print('PASS two-character password ordinary Backspace leaves one character', flush=True)
        browser.tap('r', 'KeyR', 82, text='r')
        browser.select_all()
        empty = browser.tap('Backspace', 'Backspace', 8)
        assert empty['value'] == '' and empty['cursor'] == empty['anchor'] == 0, empty
        assert empty['start'] == 0 and empty['end'] == 2, empty
        print('PASS Ctrl+A then Backspace deletes the entire two-character password', flush=True)
        for char in 'xyz':
            browser.tap(char, 'Key' + char.upper(), ord(char.upper()), text=char)
        browser.select_all()
        empty = browser.tap('Delete', 'Delete', 46)
        assert empty['value'] == '' and empty['cursor'] == empty['anchor'] == 0, empty
        assert empty['start'] == 0 and empty['end'] == 3, empty
        events = browser.evaluate('passwordEvents')
        assert events and all(event['trusted'] for event in events), events
        assert sum(event['type'] == 'keydown' and event['key'] == 'a' and event['ctrl']
                   for event in events) == 3, events
        print('PASS Ctrl+A then Delete clears all; trusted DOM events; masked paint and secure semantics every frame', flush=True)
        for char in 'ab':
            browser.tap(char, 'Key' + char.upper(), ord(char.upper()), text=char)
        def paused_keys(actions):
            browser.evaluate('globalThis.passwordPauseFrames = true')
            before = browser.until(lambda s: s.get('paused') == 1, 'frame consumption paused')
            for action in actions:
                browser.key(*action)
            assert browser.state()['frames'] == before['frames'], 'chord must precede next frame'
            browser.evaluate('globalThis.passwordPauseFrames = false')
            return browser.frames(8)

        chord = [('Control', 'ControlLeft', 17, True, 2),
                 ('a', 'KeyA', 65, True, 2), ('a', 'KeyA', 65, False, 2),
                 ('Control', 'ControlLeft', 17, False)]
        after = paused_keys(chord)
        assert after['value'] == 'ab' and after['anchor'] == 0 and after['cursor'] == 2 \
            and after['selectedPaint'] == 1 and after['lastModifiers'] == 2, after
        assert after['heldModifiers'] == after['snapshotModifiers'] == 0, after
        assert after['snapshotControl'] == 0, after
        print('PASS complete chord before next frame selects visibly; held keys and snapshot stay released', flush=True)
        replaced = browser.tap('q', 'KeyQ', 81, text='q')
        assert replaced['value'] == 'q' and replaced['lastModifiers'] == 0, replaced
        assert replaced['start'] == 0 and replaced['end'] == 2, replaced
        browser.tap('r', 'KeyR', 82, text='r')
        paused_keys(chord)
        empty = browser.tap('Backspace', 'Backspace', 8)
        assert empty['value'] == '' and empty['start'] == 0 and empty['end'] == 2, empty
        for char in 'ab':
            browser.tap(char, 'Key' + char.upper(), ord(char.upper()), text=char)
        paused_keys(chord)
        empty = browser.tap('Delete', 'Delete', 46)
        assert empty['value'] == '' and empty['start'] == 0 and empty['end'] == 2, empty
        print('PASS released Ctrl+A supports replacement, Backspace and Delete on two-character password', flush=True)
        for char in 'ab':
            browser.tap(char, 'Key' + char.upper(), ord(char.upper()), text=char)
        # A plain key queued behind a shortcut carries its own zero flags.
        mixed = paused_keys(chord + [('ArrowLeft', 'ArrowLeft', 37, True),
                                     ('ArrowLeft', 'ArrowLeft', 37, False)])
        assert mixed['lastKey'] == 263 and mixed['lastModifiers'] == 0, mixed
        assert mixed['cursor'] == mixed['anchor'] == 0 and mixed['selectedPaint'] == 0, mixed
        print('PASS shortcut followed by plain key before frame does not leak modifiers', flush=True)
        count = mixed['shortcuts']
        repeated = paused_keys(chord[:2] + [('a', 'KeyA', 65, True, 2, '', True)] + chord[2:])
        assert repeated['shortcuts'] == count + 2 and repeated['lastModifiers'] == 2, repeated
        assert repeated['cursor'] == 2 and repeated['anchor'] == 0 and repeated['selectedPaint'] == 1, repeated
        # DOM flags remain authoritative when a modifier was held before focus.
        browser.tap('ArrowLeft', 'ArrowLeft', 37)
        event_only = paused_keys(chord[1:3])
        assert event_only['cursor'] == 2 and event_only['anchor'] == 0, event_only
        assert event_only['lastModifiers'] == 2 and event_only['heldModifiers'] == 0, event_only
        browser.tap('ArrowRight', 'ArrowRight', 39)
        shifted = paused_keys([('Shift', 'ShiftLeft', 16, True, 8),
                               ('ArrowLeft', 'ArrowLeft', 37, True, 8),
                               ('ArrowLeft', 'ArrowLeft', 37, False, 8),
                               ('Shift', 'ShiftLeft', 16, False)])
        assert shifted['anchor'] == 2 and shifted['cursor'] == 1 and shifted['selectedPaint'] == 1, shifted
        assert shifted['lastModifiers'] == 1 and shifted['heldModifiers'] == shifted['snapshotModifiers'] == 0, shifted
        events = browser.evaluate('passwordEvents')
        assert events and all(event['trusted'] for event in events), events
        print('PASS repeated Ctrl+A, event-only Ctrl flags and released Shift+Left; all DOM events trusted', flush=True)
        receipt = ROOT / 'backup/password-browser-receipt.json'
        subprocess.run(['git', 'check-ignore', str(receipt)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps({'result': 'PASS', 'state': shifted,
            'trusted_events': len(events), 'cases': ['held Ctrl+A', 'UTF-8 selection/highlight',
            'replacement', 'ordinary Backspace', 'two-character selection Backspace/Delete',
            'complete chord before frame', 'mixed plain key', 'repeat', 'DOM modifier flags',
            'Shift selection', 'masking', 'secure semantics', 'held frame snapshot isolation']}, indent=2) + '\n')
    finally:
        browser.close()


def main():
    subprocess.run(['git', 'check-ignore', str(WORK / 'profile')], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        generated = WORK / 'generated'
        shutil.rmtree(generated, ignore_errors=True)
        run([ZIRAN, 'build', '--project', '--module-path', ROOT / 'tests', '--target=c', '--target-os', 'web',
             '--target-cpu', 'wasm32', '--no-main', '--define', 'PLATFORM_WEB',
             '--root', ROOT / 'tests', '-o', generated,
             ROOT / 'tests/password_canvas_browser_behavior.zi'])
        shell = WORK / 'shell.html'
        shell.write_text('''<!doctype html><meta charset="utf-8"><body>
            <div id="canvas-frame" style="width:640px;height:200px">
            <canvas id="canvas" tabindex="0" width="640" height="200"></canvas></div>
            {{{ SCRIPT }}}</body>''')
        run([os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc')),
             '-O1', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', generated,
             *sorted(generated.rglob('*.c')), '--js-library', ZIRAN_ROOT / 'web/ziran_web.js',
             '-sASYNCIFY', '-sSINGLE_FILE=1', '-sENVIRONMENT=web', '--shell-file', shell,
             '-o', WORK / 'test.html'])
        exercise(WORK / 'test.html')
    print('PASS password Canvas browser local test', flush=True)


if __name__ == '__main__':
    main()
