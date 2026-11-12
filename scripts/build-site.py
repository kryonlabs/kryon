#!/usr/bin/env python3
"""Package the live Ziran/Canvas Kryon website."""
import html
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'docs/site'
NAV = [
    ('index.html', 'Overview'), ('getting-started.html', 'Guide'),
    ('widgets.html', 'Components'), ('apps.html', 'Apps'), ('docs.html', 'Docs'),
]
MAINTAINED = [
    ('index.html', 'Kryon — Ziran UI system', 'Kryon is a reusable Ziran UI library with source-driven controls, charts, overlays and data views.'),
    ('getting-started.html', 'Getting started — Kryon', 'Add Kryon to a Ziran project, import widgets, and run the maintained local tests.'),
    ('widgets.html', 'Components — Kryon', 'Browse real light and dark Ziran component examples with source, APIs, options and status.'),
    ('modules.html', 'Modules — Kryon', 'The maintained gallery currently builds Kryon widgets, Plot charts, DataViews controls, focused inputs and overlays.'),
    ('apps.html', 'Apps and showcase — Kryon', 'Kryon Labs applications are public, open-source Ziran and Kryon projects with links to their GitHub repositories.'),
    ('docs.html', 'Docs — Kryon', 'Kryon ownership, public surface, local commands and integration boundaries.'),
    ('renderers.html', 'Platforms — Kryon', 'Kryon runs through Ziran desktop and Canvas hosts; support varies by backend and target.'),
]
REDIRECTS = {
    'api.html': ('docs.html', 'API docs moved'),
    'components.html': ('widgets.html', 'Components moved'),
    'demo.html': ('widgets.html', 'Live editor moved'),
    'live-examples.html': ('widgets.html', 'Examples moved'),
    'language.html': ('docs.html', 'Language docs moved'),
    'showcase.html': ('apps.html', 'Showcase moved'),
    'matrices.html': ('renderers.html', 'Platform matrix moved'),
    'benchmarks.html': ('docs.html', 'Benchmarks moved'),
}



def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def package_runtime(page):
    """Stream compiler payloads into shared files without changing widget code."""
    import base64
    from html.parser import HTMLParser
    runtime = SITE / 'runtime'
    runtime.mkdir(parents=True, exist_ok=True)
    assets = []

    class Package(HTMLParser):
        def __init__(self):
            super().__init__(convert_charrefs=False)
            self.output, self.mount, self.pending = None, None, ''
        def handle_starttag(self, tag, attrs):
            attrs = dict(attrs)
            if tag != 'script': return
            identity = attrs.get('id', '')
            if identity.startswith('kryon-module-'):
                self.output = (runtime / (identity.removeprefix('kryon-module-') + '.js')).open('w')
            elif attrs.get('class') == 'kryon-file':
                self.mount = attrs['data-mount']
                name = 'asset-' + str(len(assets)) + '.bin'
                assets.append([self.mount, 'runtime/' + name])
                self.output = (runtime / name).open('wb')
        def handle_data(self, data):
            if self.output is None: return
            if self.mount is None: self.output.write(data); return
            self.pending += ''.join(data.split())
            end = len(self.pending) // 4 * 4
            if end:
                self.output.write(base64.b64decode(self.pending[:end], validate=True))
                self.pending = self.pending[end:]
        def handle_endtag(self, tag):
            if tag == 'script' and self.output is not None:
                if self.pending: raise ValueError('Truncated compiler asset')
                self.output.close(); self.output = None; self.mount = None
    parser = Package()
    with page.open() as stream:
        for chunk in iter(lambda: stream.read(65536), ''): parser.feed(chunk)
    parser.close()
    assert (runtime / 'simd128.js').stat().st_size > 1000
    assert (runtime / 'baseline.js').stat().st_size > 1000
    return assets

def main():
    from gallery import PROJECT, prepare
    prepare()
    subprocess.run(['python3', str(SITE / 'demo/build_preview.py')], cwd=ROOT, check=True)
    environment = os.environ.copy()
    environment['PROJECT_EXTRA_SOURCES'] = 'build/catalog/catalog.zi'
    subprocess.run(['ziran', 'tool', 'kryon', 'build', '--profile', 'web'],
                   cwd=PROJECT, env=environment, check=True)
    assets = package_runtime(ROOT / 'build/examples/gallery/gallery-web.html')
    write(SITE / 'runtime/assets.json', __import__('json').dumps(assets) + '\n')
    write(SITE / 'current.css', 'html,body{margin:0;width:100%;height:100%;overscroll-behavior:none}#canvas-frame{width:100%;height:100%}canvas{display:block;width:100%;height:100%;outline:none}#kryon-ime{position:fixed;left:0;top:0;width:1px;height:1px;border:0;padding:0;opacity:0;pointer-events:none}#result:empty{display:none}#result:not(:empty){position:fixed;inset:auto 20px 20px;margin:0;padding:16px;border:1px solid #d4d4d4;border-radius:10px;background:#fff;color:#171717;font:14px/1.5 system-ui;white-space:pre-wrap}\n')
    with (SITE / 'runtime/simd128.js').open('rb') as stream:
        revision = hashlib.file_digest(stream, 'sha256').hexdigest()[:16]
    for name, title, description in MAINTAINED:
        write(SITE / name, '<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>' + html.escape(title) + '</title><meta name="description" content="' + html.escape(description, quote=True) + '"><link rel="stylesheet" href="current.css?v=' + revision + '"></head><body><input id="kryon-ime" type="text" tabindex="-1" aria-hidden="true" autocomplete="off" autocapitalize="off" spellcheck="false"><div id="canvas-frame"><canvas id="canvas" tabindex="0" aria-label="Kryon interactive website"></canvas></div><pre id="result" role="status"></pre><noscript>This website runs Ziran through WebAssembly. Enable JavaScript to use the live examples.</noscript><script src="current.js?v=' + revision + '"></script></body></html>\n')
    for name, (target, title) in REDIRECTS.items():
        write(SITE / name, '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=' + target + '"><title>' + title + '</title></head><body><a href="' + target + '">' + title + '</a></body></html>\n')
    print('Packaged live Ziran website: 7 routes, 101 shared widget examples')


if __name__ == '__main__': main()
