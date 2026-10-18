"""The same app.zib runs on desktop and through the browser VM renderer."""
import os
import re
from pathlib import Path
import shutil
import subprocess

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from raylib_project_test import solid_png

project = ROOT / 'build/zib-canvas-test'
project.mkdir(parents=True, exist_ok=True)
(project / 'src').mkdir(exist_ok=True)
(project / 'assets').mkdir(exist_ok=True)
source = (ROOT / 'tests/zib/app.zi').read_text()
source = ('#import "kryon/image_raster"\n'
          'startup_width: s32 = AssetWidth("assets/red.png");\n'
          'outside_width: s32 = AssetWidth("private.png");\n') + source
source = source.replace('    frames += 1', '    if startup_width != 32 || outside_width != 0 { return 1 }\n    frames += 1')
(project / 'src/app.zi').write_text(source)
solid_png(project / 'assets/red.png', 32, 32, (230, 30, 40, 255))
(project / 'ziran.toml').write_text('''[package]
name = "zib_canvas_probe"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
default_profile = "portable"
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zib"
assets = "assets"
[tool.kryon.profiles.web]
backend = "canvas"
codegen = "zib"
assets = "assets"
[tool.kryon.profiles.native]
backend = "raylib"
codegen = "zib"
assets = "assets"
''')
(project / 'ziran.local.toml').write_text(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(name, None)
env['PATH'] = str(ROOT / 'build/bin') + os.pathsep + env['PATH']


def run(command):
    subprocess.run(list(map(str, command)), cwd=project, env=env, check=True, timeout=300)


run([ZIRAN, 'lock'])
for profile in ('portable', 'web', 'native'):
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', profile])
portable = project / 'build/zib_canvas_probe-portable.zib'
web = project / 'build/zib_canvas_probe-web.zib'
native = project / 'build/zib_canvas_probe-native.zib'
(project / 'build/invalid.zib').write_bytes(b'invalid ZIB')
assert portable.read_bytes() == web.read_bytes(), 'Backend changed the portable app bundle'
assert portable.read_bytes() == native.read_bytes(), 'Native renderer changed the portable app bundle'
native_player = project / 'build/generated/native/player/zib'
run([native_player, '--check', native])
assert (native_player.parent / 'kryon-font.ttf').is_file()
assert all(re.fullmatch(r'(?:p[0-9a-f]{16}_)?(?:bundle_app|hosted_app)\.c', path.name)
           for path in (native_player.parent / 'generated/c').glob('*_app.c'))
# The web shell contains the renderer and VM, never native app/widget output.
generated = project / 'build/generated/web/player/generated/c'
assert all(re.fullmatch(r'(?:p[0-9a-f]{16}_)?(?:bundle_app|hosted_app)\.c', path.name)
           for path in generated.glob('*_app.c'))
assert not list(generated.glob('*_zib_run.c'))
for mode in ('keyboard', 'unicode'):
    run(['node', ROOT / 'tests/zib_canvas_browser.mjs',
         project / 'build/zib_canvas_probe-web.html', mode])
run(['make', '-C', ROOT, 'zib-canvas', 'ZIB_CANVAS_BUNDLE='])
run(['node', ROOT / 'tests/zib_canvas_browser.mjs', ROOT / 'build/zib-canvas/player.html',
     'file', web])
run(['node', ROOT / 'tests/zib_canvas_browser.mjs', ROOT / 'build/zib-canvas/player.html',
     'url', web])
print('Desktop/native/web identical ZIB, complete browser rendering and input passed')
