"""The same app.zx runs on desktop and through the browser VM renderer."""

from toolchain import JS_RUNTIME, pinned, kryon_only
import os
import re
from pathlib import Path
import shutil
import subprocess

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from image_test_tools import solid_png

project = ROOT / 'build/zx-canvas-test'
project.mkdir(parents=True, exist_ok=True)
(project / 'src').mkdir(exist_ok=True)
(project / 'assets').mkdir(exist_ok=True)
source = (ROOT / 'tests/zx/app.tsx').read_text()
frame = 'function Frame(frame: App.Frame): void {\n'
assert source.count(frame) == 1, 'shared ZX fixture has no unique Frame to extend'
startup = '''import { AssetWidth } from "kryon/image_raster";
let startupWidth: s32 = AssetWidth("assets/red.png");
let outsideWidth: s32 = AssetWidth("private.png");
'''
source = source.replace(frame, startup + frame +
                        '    if (startupWidth !== s32(32) || outsideWidth !== s32(0)) { WindowQuit(); return; }\n', 1)
(project / 'src/app.zi').unlink(missing_ok=True)
(project / 'src/app.tsx').write_text(source)
solid_png(project / 'assets/red.png', 32, 32, (230, 30, 40, 255))
(project / 'ziran.toml').write_text(pinned('''[package]
name = "zx_canvas_probe"
entry = "src/app.tsx"
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
codegen = "zx"
assets = "assets"
[tool.kryon.profiles.web]
backend = "canvas"
codegen = "zx"
assets = "assets"
'''))
(project / 'ziran.local.toml').write_text(kryon_only(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n'))
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(name, None)
env['PATH'] = str(ROOT / 'build/bin') + os.pathsep + env['PATH']


def run(command):
    subprocess.run(list(map(str, command)), cwd=project, env=env, check=True, timeout=300)


# The project is written fresh each run: a lock left by an earlier
# toolchain would hold checked packages this toolchain refuses.
(project / 'ziran.lock').unlink(missing_ok=True)
run([ZIRAN, 'lock'])
for profile in ('portable', 'web'):
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', profile])
portable = project / 'build/zx_canvas_probe-portable.zx'
web = project / 'build/zx_canvas_probe-web.zx'
(project / 'build/invalid.zx').write_bytes(b'invalid ZX')
assert portable.read_bytes() == web.read_bytes(), 'Backend changed the portable app bundle'
# Kryon's desktop player runs the identical bundle natively.
run([ROOT / 'build/bin/zx', '--check', portable])
# The web shell contains the renderer and VM, never native app/widget output.
generated = project / 'build/generated/web/player/generated/c'
assert all(re.fullmatch(r'(?:p[0-9a-f]{16}_)?(?:kryon__)?(?:bundle_app|hosted_app)\.c', path.name)
           for path in generated.glob('*_app.c'))
assert not list(generated.glob('*_frames_zx.c'))
for mode in ('keyboard', 'unicode'):
    run([JS_RUNTIME, ROOT / 'tests/zx_canvas_browser.mjs',
         project / 'build/zx_canvas_probe-web.html', mode])
run(['make', '-C', ROOT, 'zx-canvas', 'ZX_CANVAS_BUNDLE='])
run([JS_RUNTIME, ROOT / 'tests/zx_canvas_browser.mjs', ROOT / 'build/zx-canvas/player.html',
     'file', web])
run([JS_RUNTIME, ROOT / 'tests/zx_canvas_browser.mjs', ROOT / 'build/zx-canvas/player.html',
     'url', web])
print('Desktop/web identical ZX, complete browser rendering and input passed')
