#!/usr/bin/env python3
"""Build the shared Ziran gallery through its locked package and stage site assets."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'examples/gallery'
SITE = ROOT / 'docs/site'
ASSETS = SITE / 'assets'
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    env.pop(name, None)
command = ['ziran', 'tool', 'kryon', 'build', '--profile', 'web']
if not (PROJECT / 'ziran.local.toml').exists():
    command.append('--locked')
subprocess.run(command, cwd=PROJECT, env=env, check=True)
compiled = (PROJECT / 'build/gallery-web.html').read_text()
compiled = compiled.replace('</head>', '<style>#result{display:none}html,body,#canvas-frame{width:480px;height:320px}</style></head>', 1)
compiled += '\n<script src="../gallery-host.js"></script>\n'
(ASSETS / 'widget-gallery.html').write_text(compiled)
shutil.copyfile(ROOT / 'icons/pfp.png', ASSETS / 'catalog-image.png')
scenes = []
for source in ('scenes.zi', 'packages.zi'):
    text = (PROJECT / 'src' / source).read_text()
    for match in re.finditer(r'if index == (\d+) \{ return "([^"]+)" \}', text):
        index = int(match[1])
        local_index = index if source == 'scenes.zi' else index - 54
        implementation = re.search(r'if scene == ' + str(local_index) + r'(?!\d)', text)
        position = implementation.start() if implementation else text.index('Compose' if source == 'scenes.zi' else 'ComposePackage')
        line = text[:position].count('\n') + 1
        scenes.append({'index': index, 'name': match[2], 'source': f'https://github.com/kryonlabs/kryon/blob/master/examples/gallery/src/{source}#L{line}', 'available': index != 62})
scenes.sort(key=lambda item: item['index'])
assert [scene['index'] for scene in scenes] == list(range(63)), 'gallery scene inventory is incomplete'
(SITE / 'gallery-data.js').write_text('window.galleryScenes = ' + json.dumps(scenes, ensure_ascii=False, indent=2) + ';\n')
for asset in (ASSETS / 'widget-gallery.html', ASSETS / 'catalog-image.png', SITE / 'gallery-data.js'):
    asset.chmod(0o644)
print(f'Built web gallery: {len(scenes)} entries from the shared Ziran sources')
