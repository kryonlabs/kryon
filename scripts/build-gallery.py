#!/usr/bin/env python3
"""Build the shared Ziran gallery through its locked package and stage site assets."""
import os
from pathlib import Path
import shutil
import subprocess
from gallery import prepare

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'examples/gallery'
SITE = ROOT / 'docs/site'
ASSETS = SITE / 'assets'
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    env.pop(name, None)
modules, scenes = prepare()
env['PROJECT_EXTRA_SOURCES'] = 'build/catalog/catalog.zi'
command = ['ziran', 'tool', 'kryon', 'build', '--profile', 'web']
if not (PROJECT / 'ziran.local.toml').exists():
    command.append('--locked')
subprocess.run(command, cwd=PROJECT, env=env, check=True)
compiled = (PROJECT / 'build/gallery-web.html').read_text()
compiled = compiled.replace('</head>', '<style>#result{display:none}html,body,#canvas-frame{width:480px;height:320px}</style></head>', 1)
compiled += '\n<script src="../gallery-host.js"></script>\n'
(ASSETS / 'widget-gallery.html').write_text(compiled)
shutil.copyfile(ROOT / 'icons/pfp.png', ASSETS / 'catalog-image.png')
for asset in (ASSETS / 'widget-gallery.html', ASSETS / 'catalog-image.png', SITE / 'gallery-data.js'):
    asset.chmod(0o644)
print(f'Built web gallery: {len(scenes)} entries from the shared Ziran sources')
