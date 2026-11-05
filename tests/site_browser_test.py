#!/usr/bin/env python3
"""Check kryonlabs.com pages and the live editor in a private headless Chromium.

  python3 tests/site_browser_test.py [--site docs/site] [--screenshots DIR]

Serves the site from a local loopback server and drives only the browser it
starts, over the DevTools pipe; it needs no display. It checks navigation,
the app and module catalogs, links, images, themes, keyboard focus, narrow
layouts, and that the browser preview host compiles and draws Text,
including rotated text, without missing capabilities.
"""
import argparse
import base64
import functools
import http.server
import io
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import urljoin, urlsplit
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PAGES = ['index', 'apps', 'modules', 'widgets', 'docs', 'getting-started', 'renderers']
NAV = ['Apps', 'Modules', 'Gallery', 'Docs']
CURRENT = {'apps': 'Apps', 'modules': 'Modules', 'widgets': 'Gallery', 'docs': 'Docs'}
APPS = {'terminal', 'browser', 'github', 'shelf'}
MODULES = {'kryon', 'plot', 'data-views', 'kss', 'mingcute', 'game2d'}
# Repositories that are not public yet; the site must not send visitors there.
PRIVATE = ('github.com/kryonlabs/terminal', 'github.com/kryonlabs/browser', 'github.com/kryonlabs/github')


class Browser:
    def __init__(self, profile, chromium):
        shutil.rmtree(profile, ignore_errors=True)
        profile.mkdir(parents=True)
        environment = {key: value for key, value in os.environ.items() if key not in (
            'DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS',
            'SESSION_MANAGER')}
        environment['YUE_DESKTOP_RECOVERY'] = '0'
        # Chromium's Unix socket path must stay below the 108-byte limit.
        short = ROOT / 'build/scratch/t'
        short.mkdir(parents=True, exist_ok=True)
        environment['TMPDIR'] = str(short)
        self.log = (profile / 'chromium.log').open('wb')
        self.process = subprocess.Popen(
            ['sh', '-c', 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', 'chromium', chromium,
             '--headless', '--no-sandbox', '--disable-gpu', '--use-gl=disabled', '--disable-dev-shm-usage',
             '--no-first-run', '--no-default-browser-check', '--hide-scrollbars', '--mute-audio',
             '--remote-debugging-pipe', '--user-data-dir=' + str(profile), 'about:blank'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, env=environment,
            start_new_session=True)
        self.received = b''
        self.sequence = 0
        self.session = None
        self.errors = []
        self.console = []
        target = next(item['targetId'] for item in self.command('Target.getTargets')['targetInfos']
                      if item['type'] == 'page')
        self.session = self.command('Target.attachToTarget', targetId=target, flatten=True)['sessionId']
        for domain in ('Runtime', 'Page', 'Log'):
            self.command(domain + '.enable')

    def close(self):
        try:
            self.process.stdin.close()
        except OSError:
            pass
        try:
            os.killpg(self.process.pid, 15)
        except ProcessLookupError:
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(self.process.pid, 9)
            self.process.wait(timeout=5)
        self.log.close()

    def command(self, method, timeout=60, **params):
        self.sequence += 1
        message = {'id': self.sequence, 'method': method, 'params': params}
        if self.session:
            message['sessionId'] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b'\0')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b'\0' in self.received:
                raw, self.received = self.received.split(b'\0', 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                event = reply.get('method')
                if event == 'Runtime.exceptionThrown':
                    details = reply['params']['exceptionDetails']
                    self.errors.append((details.get('exception') or {}).get('description') or details.get('text'))
                elif event == 'Runtime.consoleAPICalled' and reply['params'].get('type') == 'error':
                    self.console.append(' '.join(str(arg.get('value', arg.get('description', '')))
                                                 for arg in reply['params']['args'])[:300])
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
        result = self.command('Runtime.evaluate', expression=expression, returnByValue=True, awaitPromise=True)
        if 'exceptionDetails' in result:
            raise RuntimeError(str(result['exceptionDetails'])[:1200])
        return result.get('result', {}).get('value')

    def until(self, expression, what, seconds=30):
        deadline = time.monotonic() + seconds
        value = None
        while time.monotonic() < deadline:
            value = self.evaluate(expression)
            if value:
                return value
            time.sleep(.1)
        raise AssertionError(what + ': timed out (last %r)' % (value,))

    def viewport(self, width, height, scale=1, mobile=False):
        self.command('Emulation.setDeviceMetricsOverride', width=width, height=height,
                     deviceScaleFactor=scale, mobile=mobile)

    def open(self, url, ready="document.readyState === 'complete'"):
        self.command('Page.navigate', url=url)
        self.until(ready, 'load ' + url)

    def key(self, key, code=None, keycode=0, shift=False):
        modifiers = 8 if shift else 0
        for kind in ('keyDown', 'keyUp'):
            self.command('Input.dispatchKeyEvent', type=kind, key=key, code=code or key,
                         windowsVirtualKeyCode=keycode, modifiers=modifiers)

    def png(self, clip=None, full=False):
        params = {'format': 'png'}
        if clip:
            params['clip'] = dict(clip, scale=1)
        if full:
            self.evaluate('''(async () => {
                await document.fonts.ready;
                await Promise.all([...document.images].map(img => {
                    img.loading = 'eager';
                    return img.decode().catch(() => {});
                }));
                await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            })()''')
            params['captureBeyondViewport'] = True
        return base64.b64decode(self.command('Page.captureScreenshot', **params)['data'])

    def element_png(self, selector):
        clip = self.evaluate('''(() => {const r=document.querySelector(%s).getBoundingClientRect();
            return {x:r.x+scrollX,y:r.y+scrollY,width:r.width,height:r.height};})()''' % json.dumps(selector))
        return self.png(clip)


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def serve(site):
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(QuietHandler, directory=str(site)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def ink_box(png, background_tolerance=60):
    """Bounding box of pixels clearly darker than the preview background."""
    from PIL import Image
    image = Image.open(io.BytesIO(png)).convert('L')
    width, height = image.size
    pixels = image.load()
    background = sorted(pixels[x, y] for x in (2, width - 3) for y in (2, height - 3))[2]
    xs, ys = [], []
    for y in range(height):
        for x in range(width):
            if pixels[x, y] < background - background_tolerance:
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


ROTATED_SOURCE = '''using UI :: #import "kryon/Widgets";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    Box(session, BoxProps.{.key = 1, .bounds = viewport, .fill = Color.{255, 255, 255, 255}})
    Text(session, TextProps.{.key = 2, .bounds = .{150.0, 30.0, 260.0, 44.0}, .text = "ROTATED TEXT",
        .font = 34, .rotation = ROTATION})
    return 0
}
'''


def check_preview_host(browser, base, shots):
    """The browser preview host runs bundles that import RasterTextRotated."""
    browser.viewport(1440, 1000)
    browser.open(base + 'demo.html')
    status = "document.getElementById('status')?.textContent"
    browser.until(status + " === 'Live · changes compiled' || " + status + " === 'Check the source'",
                  'default editor source did not finish', 150)
    diagnostics = browser.evaluate("document.getElementById('diagnostics').textContent")
    assert browser.evaluate(status) == 'Live · changes compiled', diagnostics
    assert 'Unavailable preview capability' not in diagnostics, diagnostics
    boxes = {}
    for rotation in ('0.0', '90.0'):
        source = ROTATED_SOURCE.replace('ROTATION', rotation)
        browser.evaluate('''(() => {const editor=document.getElementById('source-editor');
            editor.value=%s;editor.dispatchEvent(new Event('input'));})()''' % json.dumps(source))
        time.sleep(.3)
        browser.until(status + " === 'Live · changes compiled' && document.getElementById('status').dataset.state !== 'busy'"
                      " || " + status + " === 'Check the source'", 'rotated source did not compile', 120)
        diagnostics = browser.evaluate("document.getElementById('diagnostics').textContent")
        assert browser.evaluate(status) == 'Live · changes compiled', diagnostics
        time.sleep(.6)
        png = browser.element_png('#live-preview')
        if shots:
            (shots / ('preview-rotation-%s.png' % rotation.split('.')[0])).write_bytes(png)
        boxes[rotation] = ink_box(png)
        assert boxes[rotation], 'no text was painted at rotation ' + rotation
    _, _, flat_width, flat_height = boxes['0.0']
    _, _, turned_width, turned_height = boxes['90.0']
    assert flat_width > flat_height * 2, boxes
    assert turned_height > turned_width * 2, boxes
    assert abs(turned_height - flat_width) <= max(8, flat_width // 5), boxes
    print('preview host: default source, horizontal and RasterTextRotated text render', boxes)

    browser.open(base + 'widgets.html#plot/4')
    browser.until("document.getElementById('preview-status')?.textContent.startsWith('Live')", 'gallery preview', 90)
    for scene in ('kryon/27', 'plot/4', 'data_views/2'):
        browser.evaluate("location.hash = %s" % json.dumps('#' + scene))
        browser.until("document.getElementById('compile') && !document.getElementById('compile').disabled",
                      'gallery compiler for ' + scene, 120)
        browser.evaluate("document.querySelector('[data-view=\"split\"]').click()")
        browser.evaluate("document.getElementById('compile').click()")
        browser.until(status + " === 'Live · changes compiled' || " + status + " === 'Check the source'",
                      'gallery compile for ' + scene, 120)
        diagnostics = browser.evaluate("document.getElementById('diagnostics').textContent")
        assert browser.evaluate(status) == 'Live · changes compiled', (scene, diagnostics)
    print('preview host: gallery Text, CandleChart and TableView sources compile and run')


def page_report(browser):
    return browser.evaluate(r'''(() => {
      const nav = document.getElementById('primary-nav');
      const navLinks = nav ? [...nav.querySelectorAll('a')].map(a => ({text: a.textContent.trim(), href: a.getAttribute('href'), current: a.getAttribute('aria-current')})) : [];
      const links = [...document.querySelectorAll('a[href]')].map(a => ({href: a.getAttribute('href'), text: a.textContent.trim(), label: a.getAttribute('aria-label') || ''}));
      const images = [...document.images].map(img => ({src: img.getAttribute('src'), alt: img.getAttribute('alt'), ok: img.complete && img.naturalWidth > 0, loading: img.loading}));
      const ids = [...document.querySelectorAll('[id]')].map(node => node.id);
      return {title: document.title, h1: document.querySelectorAll('h1').length, navLinks, links, images, ids,
        lang: document.documentElement.lang, main: !!document.querySelector('main#main, main'),
        description: document.querySelector('meta[name=description]')?.content || '',
        overflow: document.documentElement.scrollWidth - innerWidth};
    })()''')


def check_links(site, base, pages):
    """Every same-site link resolves, and every fragment names an element."""
    checked = {}
    for name, report in pages.items():
        for link in report['links']:
            href = link['href']
            if href.startswith(('mailto:', 'javascript:')):
                continue
            url = urljoin(base + name + '.html', href)
            if not url.startswith(base):
                continue
            path, fragment = url.split('#', 1) if '#' in url else (url, '')
            if path not in checked:
                with urllib.request.urlopen(path) as response:
                    checked[path] = response.status
            assert checked[path] == 200, (name, href, checked[path])
            if fragment:
                target = urlsplit(path).path.lstrip('/') or 'index.html'
                target_name = target[:-5] if target.endswith('.html') else target
                if target_name in pages:
                    assert fragment in pages[target_name]['ids'] or target_name == 'widgets', (name, href)
    return len(checked)


def check_pages(browser, base, site, shots):
    reports = {}
    browser.viewport(1366, 900)
    for name in PAGES:
        browser.open(base + name + '.html')
        browser.until("[...document.images].every(img => img.loading === 'lazy' || img.complete)", 'images on ' + name, 20)
        report = page_report(browser)
        reports[name] = report
        assert report['lang'] == 'en' and report['h1'] == 1 and report['main'], (name, report['h1'])
        assert report['description'], name
        texts = [link['text'] for link in report['navLinks']]
        assert texts == NAV, (name, texts)
        current = [link['text'] for link in report['navLinks'] if link['current'] == 'page']
        assert current == ([CURRENT[name]] if name in CURRENT else []), (name, current)
        for image in report['images']:
            assert image['alt'] is not None, (name, image['src'])
        assert report['overflow'] <= 0, (name, 'desktop overflow', report['overflow'])
        for link in report['links']:
            assert not any(private in link['href'] for private in PRIVATE), (name, link['href'])
            assert not re.search(r'/releases|\.deb$|\.apk$|\.tar\.gz$|download', link['href']), (name, link['href'])
            assert link['text'] or link['label'], (name, 'unlabelled link', link['href'])
    browser.open(base + 'showcase.html')
    browser.until("location.pathname.endsWith('/apps.html')", 'legacy projects route')
    with urllib.request.urlopen(base + 'showcase-data.json') as response:
        assert response.status == 200 and json.load(response), 'showcase data must remain available'
    count = check_links(site, base, reports)
    print('pages: navigation, titles, descriptions, images, %d same-site links and anchors' % count)

    # Every image, including lazily loaded previews, decodes once scrolled to.
    for name in PAGES:
        browser.open(base + name + '.html')
        browser.evaluate('(async () => { for (let y = 0; y < document.body.scrollHeight; y += 500) {'
                         ' scrollTo(0, y); await new Promise(r => setTimeout(r, 40)); } scrollTo(0, 0); })()')
        browser.until("[...document.images].every(img => img.complete && img.naturalWidth > 0)", 'decoded images on ' + name, 20)

    home = reports['index']
    hrefs = [link['href'] for link in home['links']]
    actions = browser.evaluate('''(() => { location.href; return null })()''')
    browser.open(base + 'index.html')
    actions = browser.evaluate("[...document.querySelectorAll('.hero .actions a')].map(a => a.getAttribute('href'))")
    assert actions[:2] == ['apps.html', 'modules.html'] and 'demo.html' in actions, actions
    assert browser.evaluate("!!document.querySelector('iframe[src=\"demo.html?mini\"]')")
    assert not any('ziran.kryonlabs.com' in href for href in hrefs), 'homepage promotes the language'
    previews = browser.evaluate("[...document.querySelectorAll('.hero img')].map(img => img.getAttribute('src'))")
    assert len(previews) >= 3, previews
    waozi = [href for href in hrefs if 'waozi.xyz' in href]
    assert len(waozi) <= 1 and browser.evaluate(
        "[...document.querySelectorAll('a[href*=\"waozi.xyz\"]')].every(a => a.closest('footer'))"), waozi

    browser.open(base + 'apps.html')
    apps = browser.evaluate('''[...document.querySelectorAll('[data-app]')].map(node => ({id: node.dataset.app,
      availability: node.dataset.availability, downloads: node.querySelectorAll('a[download], [data-download]').length,
      facts: [...node.querySelectorAll('dt')].map(dt => dt.textContent.trim())}))''')
    details = {app['id']: app for app in apps if app['facts']}
    assert set(details) == APPS, details.keys()
    for app in details.values():
        assert app['availability'] in ('unreleased', 'source'), app
        assert app['downloads'] == 0, app
        assert {'Platform', 'Status', 'Availability'} <= set(app['facts']), app
    assert details['shelf']['availability'] == 'source'
    assert browser.evaluate("!!document.querySelector('#shelf a[href=\"https://github.com/kryonlabs/shelf\"]')")

    browser.open(base + 'modules.html')
    modules = browser.evaluate('''[...document.querySelectorAll('section[data-module]')].map(node => ({id: node.dataset.module,
      visual: !!node.querySelector('img, svg, iframe'), demo: [...node.querySelectorAll('a')].some(a => /widgets\\.html#|demo\\.html/.test(a.getAttribute('href'))),
      source: [...node.querySelectorAll('a')].some(a => a.getAttribute('href').startsWith('https://github.com/kryonlabs/'))}))''')
    assert {module['id'] for module in modules} == MODULES, modules
    for module in modules:
        assert module['visual'] and module['source'], module
    for module in ('kryon', 'plot', 'data-views'):
        assert next(m for m in modules if m['id'] == module)['demo'], module
    text = browser.evaluate('document.querySelector("main").innerText')
    assert 'Ziran language' not in text, 'the language is not a catalog module'
    print('catalogs: %d apps with honest availability and %d visual modules' % (len(details), len(modules)))

    # Light and dark themes switch, persist, and reach the embedded editor.
    browser.open(base + 'index.html?theme=light')
    browser.evaluate("localStorage.removeItem('kryon-theme')")
    browser.open(base + 'index.html')
    light = browser.evaluate('getComputedStyle(document.body).backgroundColor')
    browser.evaluate("document.querySelector('.theme-toggle').click()")
    assert browser.evaluate('document.documentElement.dataset.theme') == 'dark'
    dark = browser.evaluate('getComputedStyle(document.body).backgroundColor')
    assert light != dark, (light, dark)
    assert browser.evaluate("document.querySelector('.theme-toggle').getAttribute('aria-pressed')") == 'true'
    browser.open(base + 'apps.html')
    assert browser.evaluate('document.documentElement.dataset.theme') == 'dark', 'theme did not persist'
    if shots:
        for name in ('index', 'apps', 'modules'):
            browser.open(base + name + '.html')
            (shots / (name + '-dark.png')).write_bytes(browser.png(full=True))
    browser.evaluate("document.querySelector('.theme-toggle').click()")
    assert browser.evaluate('document.documentElement.dataset.theme') == 'light'
    print('themes: light and dark switch and persist')

    # Keyboard: skip link first, then the primary navigation, with a visible focus ring.
    browser.open(base + 'apps.html')
    browser.key('Tab', keycode=9)
    assert browser.evaluate("document.activeElement.classList.contains('skip-link')")
    assert browser.evaluate("document.activeElement.getBoundingClientRect().top >= 0")
    browser.key('Enter', keycode=13)
    browser.until("location.hash === '#main'", 'skip link')
    order = []
    browser.open(base + 'apps.html')
    for _ in range(8):
        browser.key('Tab', keycode=9)
        order.append(browser.evaluate("document.activeElement.textContent.trim() || document.activeElement.getAttribute('aria-label')"))
    for item in NAV:
        assert item in order, (item, order)
    ring = browser.evaluate("getComputedStyle(document.activeElement).outlineStyle")
    assert ring not in ('none', ''), ring
    print('keyboard: skip link, navigation order and focus ring')

    # Narrow layout: no sideways scrolling, and the menu opens, closes and traps nothing.
    browser.viewport(390, 844, 2, True)
    for name in PAGES + ['demo']:
        browser.open(base + name + '.html')
        time.sleep(.2)
        overflow = browser.evaluate('document.documentElement.scrollWidth - innerWidth')
        assert overflow <= 0, (name, 'narrow overflow', overflow)
        if shots and name in ('index', 'apps', 'modules'):
            (shots / (name + '-narrow.png')).write_bytes(browser.png(full=True))
    for name in PAGES + ['demo']:
        browser.open(base + name + '.html')
        for width in (320, 360, 390, 650, 651, 768, 850, 1000, 1366):
            browser.viewport(width, 844, mobile=width <= 650)
            overflow = browser.evaluate('document.documentElement.scrollWidth - innerWidth')
            assert overflow <= 0, (name, width, 'responsive overflow', overflow)
    print('responsive: all eight pages fit 320–1366px across nine widths')
    browser.viewport(390, 844, 2, True)
    browser.open(base + 'modules.html')
    assert browser.evaluate("getComputedStyle(document.querySelector('.menu-button')).display") != 'none'
    assert not browser.evaluate("document.getElementById('primary-nav').offsetHeight")
    browser.evaluate("document.querySelector('.menu-button').click()")
    assert browser.evaluate("document.querySelector('.menu-button').getAttribute('aria-expanded')") == 'true'
    assert browser.evaluate("document.getElementById('primary-nav').offsetHeight") > 100
    browser.key('Escape', keycode=27)
    assert browser.evaluate("document.querySelector('.menu-button').getAttribute('aria-expanded')") == 'false'
    assert browser.evaluate("document.activeElement.classList.contains('menu-button')")
    assert not browser.evaluate("document.getElementById('primary-nav').offsetHeight")
    print('narrow: no horizontal overflow; the menu opens and closes with Escape')

    if shots:
        browser.viewport(1366, 900)
        for name in PAGES:
            browser.open(base + name + '.html')
            (shots / (name + '.png')).write_bytes(browser.png(full=True))
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, default=ROOT / 'docs/site')
    parser.add_argument('--chromium', default=os.environ.get('CHROMIUM', 'chromium'))
    parser.add_argument('--screenshots', type=Path)
    parser.add_argument('--skip-preview', action='store_true', help='skip the compiled preview host checks')
    parser.add_argument('--only-preview', action='store_true', help='run only the compiled preview host checks')
    args = parser.parse_args()
    site = args.site.resolve()
    shots = args.screenshots.resolve() if args.screenshots else None
    if shots:
        shots.mkdir(parents=True, exist_ok=True)
    server = serve(site)
    base = 'http://127.0.0.1:%d/' % server.server_address[1]
    scratch = ROOT / 'build/scratch/site-browser'
    scratch.mkdir(parents=True, exist_ok=True)
    browser = Browser(Path(tempfile.mkdtemp(prefix='p', dir=scratch)), args.chromium)
    try:
        if not args.only_preview:
            check_pages(browser, base, site, shots)
        if not args.skip_preview:
            check_preview_host(browser, base, shots)
        assert not browser.errors, browser.errors
        print('kryonlabs.com local browser tests passed')
    finally:
        browser.close()
        server.shutdown()


if __name__ == '__main__':
    main()
