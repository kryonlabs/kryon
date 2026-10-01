#!/usr/bin/env python3
"""Build the browser compiler and Ziran Canvas host using package overrides.

Run from the demo project. The normal `ziran pkg path` resolution honors its
lock and canonical local overrides; no vendored source is changed.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

project = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ziran', default='ziran')
parser.add_argument('--emcc', default=shutil.which('emcc'))
args = parser.parse_args()
if not args.emcc:
    parser.error('emcc is required; activate the Emscripten SDK first')
env = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY'):
    env.pop(name, None)

def package(name):
    return Path(subprocess.check_output([args.ziran, 'pkg', 'path', name], cwd=project, env=env, text=True).strip())

def run(command):
    subprocess.run(list(map(str, command)), cwd=project, env=env, check=True)

ziran = package('ziran')
kryon = package('kryon')
assets = project.parent / 'assets'
build = kryon / 'build/scratch/web-preview'
generated = build / 'host'
build.mkdir(parents=True, exist_ok=True)
env.setdefault('EM_CACHE', str(ziran / 'build/emscripten-cache'))
run(['python3', ziran / 'scripts/build_playground.py', '--emcc', args.emcc,
     '--output-dir', assets,
     '--embed-file', f'{kryon / "src/ui"}@/kryon/ui',
     '--embed-file', f'{project / "src/preview_entry.zi"}@/preview_entry.zi',
     '--embed-file', f'{project / "src/preview_style.zi"}@/preview_style.zi',
     '--embed-file', f'{project / "src/preview_protocol.zi"}@/preview_protocol.zi'])
run([ziran / 'build/bin/zi2c', '--prune-stale', '--define', 'PLATFORM_WEB', '--root', project / 'src',
     '--module-path', kryon / 'src/ui', '--module-path', kryon / 'src/backend',
     '--module-path', f'kryon={kryon / "src/ui"}',
     '--module-path', f'kryon={kryon / "src/backend"}',
     '--entry', 'preview_host:main', '-o', generated, project / 'src/preview_host.zi'])
run([args.emcc, '-O2', '-I' + str(ziran / 'include'), '-iquote', generated,
     *sorted(generated.glob('*.c')), ziran / 'build/playground64/libziran.a', '-lm',
     '--js-library', ziran / 'web/ziran_web.js',
     '--embed-file', f'{kryon / "assets/fonts/LiberationSans-Regular.ttf"}@/kryon-font.ttf',
     '-sMEMORY64=2', '-sASYNCIFY', '-sSINGLE_FILE=1', '-sEXIT_RUNTIME=1',
     '-sALLOW_MEMORY_GROWTH=1', '-sMAXIMUM_MEMORY=536870912', '-sSTACK_SIZE=8388608',
     '-sENVIRONMENT=web', '-sEXPORTED_RUNTIME_METHODS=FS',
     '--shell-file', project / 'preview-shell.html', '-o', assets / 'preview.html'])
for name in ('preview.html', 'playground-runtime.js', 'playground-runtime.wasm'):
    (assets / name).chmod(0o644)
print('Built the browser compiler and Ziran Canvas preview host')
