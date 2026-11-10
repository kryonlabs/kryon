#!/usr/bin/env python3
"""Build the desktop gallery and capture every Ziran scene in both palettes."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tomllib

from PIL import Image

from gallery import prepare

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'examples/gallery'
SITE = ROOT / 'docs/site'
ASSETS = SITE / 'assets/components'
CAPTURE_STATE = ASSETS / 'capture-state.json'


def clean_environment():
    env = os.environ.copy()
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
                 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_STARTUP_ID',
                 'XDG_SESSION_ID', 'XDG_SESSION_TYPE'):
        env.pop(name, None)
    env.update(YUE_DESKTOP_RECOVERY='0', SDL_RENDER_DRIVER='software',
               LIBGL_ALWAYS_SOFTWARE='1', ZIRAN_PAR_THREADS='1')
    return env


def digest_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def scene_digest(scene):
    digest = hashlib.sha256()
    for path in (ROOT / scene['sourcePath'], SITE / scene['code']):
        digest.update(str(path.relative_to(ROOT)).encode() + b'\0')
        digest.update(path.read_bytes())
    return digest.hexdigest()


SITE_ONLY_MANIFEST_KEYS = {'categories', 'keyboard', 'notes', 'summaries', 'unavailable'}


def project_digests():
    paths = [PROJECT / 'ziran.toml',
             PROJECT / 'ziran.lock', PROJECT / 'templates/frame.zi',
             ROOT / 'scripts/build-gallery.py',
             ROOT / 'scripts/export_theme_tokens.zi', SITE / 'theme-tokens.css']
    paths.extend((PROJECT / 'src').glob('*.zi'))
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(str(path.relative_to(ROOT)).encode() + b'\0')
        digest.update(path.read_bytes())
    manifest = tomllib.loads((PROJECT / 'modules.toml').read_text())
    capture_manifest = {
        'modules': [
            {key: value for key, value in module.items() if key not in SITE_ONLY_MANIFEST_KEYS}
            for module in manifest.get('modules', [])
        ]
    }
    digest.update(json.dumps(capture_manifest, sort_keys=True, separators=(',', ':')).encode())
    metadata_digest = digest_file(PROJECT / 'modules.toml')
    return digest.hexdigest(), metadata_digest


def capture_is_current(path, state, scene_source_digest, project_source_digest,
                       metadata_digest, binary_digest):
    expected = {'source': scene_source_digest, 'global-source': project_source_digest,
                'binary': binary_digest}
    if not path.is_file():
        return False
    record = state.get(path.name)
    if not isinstance(record, dict) or any(record.get(key) != value for key, value in expected.items()):
        return False
    if 'metadata' not in record:
        return False
    try:
        with Image.open(path) as image:
            return image.size == (480, 320)
    except OSError:
        return False


def capture(binary, scene, theme, env):
    local = scene['id'].split('/')[-1]
    shot = ASSETS / f'{scene["moduleId"]}-{local}-{theme}.webp'
    temporary = shot.with_suffix('.png')
    child_env = {**env, 'KRYON_CATALOG_SCENE': str(scene['index']),
                 'KRYON_CATALOG_THEME': theme, 'KRYON_CAPTURE_PATH': str(temporary)}
    subprocess.run(['xvfb-run', '-a', str(binary)], cwd=ROOT, env=child_env, check=True)
    with Image.open(temporary) as image:
        if image.size != (480, 320):
            raise SystemExit(f'{shot}: expected 480x320, got {image.size}')
        image.save(shot, 'WEBP', quality=90, method=6)
    temporary.unlink()


def main():
    env = clean_environment()
    modules, scenes = prepare()
    env['PROJECT_EXTRA_SOURCES'] = 'build/catalog/catalog.zi'
    command = ['ziran', 'tool', 'kryon', 'build', '--profile', 'desktop']
    if not (PROJECT / 'ziran.local.toml').exists():
        command.append('--locked')
    subprocess.run(command, cwd=PROJECT, env=env, check=True)
    binary = PROJECT / 'build/gallery-desktop'
    ASSETS.mkdir(parents=True, exist_ok=True)
    binary_digest = digest_file(binary)
    project_source_digest, metadata_digest = project_digests()
    try:
        state = json.loads(CAPTURE_STATE.read_text())
        if not isinstance(state, dict): state = {}
    except (OSError, ValueError):
        state = {}
    expected = set()
    regenerated = 0
    for scene in scenes:
        for theme in ('light', 'dark'):
            local = scene['id'].split('/')[-1]
            shot = ASSETS / f'{scene["moduleId"]}-{local}-{theme}.webp'
            scene_source_digest = scene_digest(scene)
            if not capture_is_current(shot, state, scene_source_digest, project_source_digest,
                                      metadata_digest, binary_digest):
                capture(binary, scene, theme, env)
                regenerated += 1
            state[shot.name] = {'source': scene_source_digest, 'global-source': project_source_digest,
                                'metadata': metadata_digest, 'binary': binary_digest}
            expected.add(shot)
    for path in ASSETS.glob('*'):
        if path not in expected and path != CAPTURE_STATE:
            path.unlink()
    expected_names = {path.name for path in expected}
    state = {name: record for name, record in state.items() if name in expected_names}
    CAPTURE_STATE.write_text(json.dumps(state, indent=2, sort_keys=True) + '\n')
    (SITE / 'gallery-data.js').chmod(0o644)
    print(f'Verified {len(scenes) * 2} Ziran captures from {len(scenes)} scenes; regenerated {regenerated}')


if __name__ == '__main__':
    main()
