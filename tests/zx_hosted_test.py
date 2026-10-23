"""Build standalone AppContext ZXs and exercise browser app persistence."""
import json
import os
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from raylib_project_test import solid_png

output = ROOT / 'build/scratch/zx-hosted'
project = output / 'project'
(project / 'src').mkdir(parents=True, exist_ok=True)
(project / 'assets').mkdir(exist_ok=True)
(project / 'src/app.zi').write_text((ROOT / 'tests/zx/hosted.zi').read_text())
solid_png(project / 'assets/red.png', 32, 32, (230, 30, 40, 255))
(project / 'ziran.toml').write_text('''[package]
name = "hosted_probe"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
''')
(project / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(
    f'{name} = {json.dumps(str(path))}\n' for name, path in [('kryon', ROOT), ('ziran', ZIRAN_ROOT)]))
env = os.environ.copy()
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(key, None)
env['ZIRAN_ROOT'] = str(ZIRAN_ROOT)
subprocess.run([str(ZIRAN), 'lock', '--offline'], cwd=project, env=env, check=True)
for name, defines in [('hosted', []), ('hosted_other', ['--define', 'HOSTED_OTHER'])]:
    subprocess.run([str(ZIRAN), 'bundle', '--project', '--offline', '--define', 'KRYON_HOSTED_UI',
                    *defines, '--entry', 'app:main', '--asset-dir', 'assets=assets',
                    '-o', str(output / f'{name}.zx'), 'src/app.zi'],
                   cwd=project, env=env, check=True, timeout=60)
    subprocess.run([str(ROOT / 'build/zx-raylib/zx'), '--check', str(output / f'{name}.zx')],
                   env=env, check=True, timeout=30)
(output / 'invalid.zx').write_bytes(b'invalid ZX')
subprocess.run([sys.executable, str(ROOT / 'tests/zx_hosted_native_test.py')],
               env=env, check=True, timeout=100)
subprocess.run(['node', str(ROOT / 'tests/zx_canvas_browser.mjs'),
                str(ROOT / 'build/zx-canvas/player.html'), 'hosted', str(output / 'hosted.zx')],
               env=env, check=True, timeout=120)
print('AppContext bundles run through the generic standalone browser host')
