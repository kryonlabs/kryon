"""Build a generic Android player and exercise only an explicit test emulator."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import time
import zipfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from raylib_project_test import png_pixels, rgb, solid_png

p = argparse.ArgumentParser()
target = p.add_mutually_exclusive_group()
target.add_argument('--serial', help='An explicitly selected, disposable emulator-* device')
target.add_argument('--build-only', action='store_true', help='Build and inspect the APK; never invoke adb')
p.add_argument('--skip-build', action='store_true')
p.add_argument('--gallery-index', type=Path)
a = p.parse_args()
project = ROOT / 'build/zib-android-test'
project.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(name, None)
env['YUE_DESKTOP_RECOVERY'] = '0'
env.setdefault('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))


def run(command, **kw):
    return subprocess.run(list(map(str, command)), cwd=project, env=env,
                          check=True, timeout=600, **kw)


if not a.skip_build:
    # Only these test-owned scaffold outputs are regenerated; cached ABI
    # objects stay in place and no application source checkout is touched.
    for relative in ('droid/app/build.gradle', 'droid/app/src/main/AndroidManifest.xml',
                     'droid/app/src/main/cpp/CMakeLists.txt',
                     'droid/app/src/main/java/com/kryon/zibtest/MainActivity.java'):
        (project / relative).unlink(missing_ok=True)
    (project / 'src').mkdir(exist_ok=True)
    (project / 'assets').mkdir(exist_ok=True)
    (project / 'src/app.zi').write_bytes((ROOT / 'tests/zib/mobile_app.zi').read_bytes())
    solid_png(project / 'assets/red.png', 32, 32, (230, 30, 40, 255))
    (project / 'ziran.toml').write_text('''[package]
name = "zib_android_probe"
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
default_profile = "android"
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zib"
assets = "assets"
[tool.kryon.profiles.android]
backend = "android"
codegen = "zib"
assets = "assets"
[tool.kryon.android]
package = "com.kryon.zibtest"
abis = "x86_64,arm64-v8a,armeabi-v7a"
min_sdk = "21"
''')
    (project / 'ziran.local.toml').write_text(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    run([ZIRAN, 'lock'])
    for profile in ('portable', 'android'):
        run([ZIRAN, 'tool', 'kryon', 'build', '--profile', profile])

portable = project / 'build/zib_android_probe-portable.zib'
android = project / 'build/zib_android_probe-android.zib'
if not portable.exists():
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', 'portable'])
assert portable.read_bytes() == android.read_bytes(), 'Android changed the guest bundle'
apk = project / 'droid/app/build/outputs/apk/debug/app-debug.apk'
with zipfile.ZipFile(apk) as archive:
    assert archive.read('assets/app.zib') == portable.read_bytes()
    assert archive.read('assets/kryon-font.ttf')
    for abi in ('arm64-v8a', 'armeabi-v7a', 'x86_64'):
        assert f'lib/{abi}/libmain.so' in archive.namelist(), abi
generated = list((project / 'droid/app/.cxx').rglob('ziran/generated/*_app.c'))
# The generic BundleApp host is native; the selected product app is not.
assert all(re.fullmatch(r'(?:p[0-9a-f]{16}_)?(?:bundle_app|hosted_app)\.c', path.name)
           for path in generated), 'The APK compiled the guest app natively'
print('Android three-ABI generic APK, embedded assets and identical desktop ZIB passed', flush=True)
if a.build_only or not a.serial:
    raise SystemExit(0)
assert a.serial.startswith('emulator-'), 'This test operates only a disposable emulator'
adb = Path(env['ANDROID_HOME']) / 'platform-tools/adb'
command = [adb, '-s', a.serial]
assert run([*command, 'shell', 'getprop', 'ro.kernel.qemu'], capture_output=True).stdout.strip() == b'1'
package = 'com.kryon.zibtest'
run([*command, 'install', '-r', apk], capture_output=True)
run([*command, 'shell', 'am', 'start', '-W', '-n', package + '/.MainActivity'], capture_output=True)
try:
    def capture(name):
        focused = run([*command, 'shell', 'dumpsys', 'activity', 'activities'], capture_output=True).stdout
        assert any(package.encode() in line for line in focused.splitlines()
                   if b'mResumedActivity' in line or b'topResumedActivity' in line), 'Test app is not foreground'
        path = project / (name + '.png')
        path.write_bytes(run([*command, 'exec-out', 'screencap', '-p'], capture_output=True).stdout)
        return png_pixels(path)

    time.sleep(3)
    before = capture('initial')
    density = run([*command, 'shell', 'wm', 'density'], capture_output=True).stdout.decode()
    scale = int(re.findall(r'(?:Physical|Override) density: (\d+)', density)[-1]) / 160
    red = [(x, y) for y in range(before[1]) for x in range(before[0])
           if rgb(before, x, y) == (230, 30, 40)]
    assert red, 'Embedded image did not render'
    top = min(y for x, y in red) - round(180 * scale)
    assert abs(min(x for x, y in red) - 200 * scale) <= 2, 'Density or viewport origin is wrong'
    run([*command, 'shell', 'input', 'tap', str(round(100 * scale)), str(round(70 * scale + top))])
    time.sleep(.5)
    clicked = capture('clicked')
    assert before[3] != clicked[3], 'Touch did not update bundle state'
    run([*command, 'shell', 'input', 'tap', str(round(100 * scale)), str(round(320 * scale + top))])
    time.sleep(1)
    run([*command, 'shell', 'input', 'text', 'portable'])
    time.sleep(2)
    capture('typing')
    edited = capture('edited')
    # The guest fills this progress bar only after exactly one click and the
    # exact expected text. It stays above the system keyboard so this checks
    # guest state rather than keyboard pixels or an overview thumbnail.
    x = round(120 * scale)
    y = round(252 * scale + top)
    expected = rgb(edited, x, y)
    assert expected != rgb(before, x, y), 'Exact text and one click did not fill the guest state indicator'
    for name in ('resumed', 'resumed-again'):
        run([*command, 'shell', 'input', 'keyevent', 'KEYCODE_HOME'])
        time.sleep(.5)
        run([*command, 'shell', 'am', 'start', '-W', '-n', package + '/.MainActivity'], capture_output=True)
        deadline = time.monotonic() + 15
        while True:
            resumed_again = capture(name)
            if rgb(resumed_again, round(250 * scale), round(230 * scale + top)) == (230, 30, 40):
                break
            assert time.monotonic() < deadline, 'App frame did not return after pause/resume'
            time.sleep(.2)
        assert rgb(resumed_again, x, y) == expected, 'Pause/resume lost exact guest state: ' + name
    print('Android emulator ZIB image, density, touch, exact text and pause/resume passed')
    def results():
        log = run([*command, 'logcat', '-d', '-v', 'brief', '-s', 'ZIB:I'], capture_output=True).stdout
        return log.count(b'Bundle loaded'), log.count(b'Bundle rejected')

    run([*command, 'shell', 'run-as', package, 'mkdir', '-p', 'files'])
    private = run([*command, 'shell', 'run-as', package, 'pwd'], capture_output=True).stdout.decode().strip()
    transport = '/data/local/tmp/kryon-zib-test.zib'
    def open_file(bundle, accepted=True):
        previous = results()[0 if accepted else 1]
        run([*command, 'push', bundle, transport], capture_output=True)
        run([*command, 'shell', shlex.join(['run-as', package, 'sh', '-c',
             'cat /data/local/tmp/kryon-zib-test.zib > files/selected.zib'])])
        run([*command, 'shell', 'am', 'start', '-W', '-a', 'android.intent.action.VIEW',
             '-d', 'file://' + private + '/files/selected.zib', '-t', 'application/vnd.ziran.bundle',
             '-n', package + '/.Zib'], capture_output=True)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if results()[0 if accepted else 1] > previous:
                time.sleep(.4)
                return
            time.sleep(.2)
        raise AssertionError('Android did not accept/reject the selected bundle as expected')

    invalid = project / 'invalid.zib'
    invalid.write_bytes(b'invalid ZIB')
    open_file(invalid, False)
    rejected = capture('invalid-file')
    assert rgb(rejected, x, y) == rgb(resumed_again, x, y), 'Invalid file destroyed app state'
    open_file(portable)
    reopened = capture('file-opened')
    assert rgb(reopened, x, y) == rgb(before, x, y), 'A valid downloaded file did not start a fresh instance'
    print('Android standard file intent and invalid-file state preservation passed', flush=True)
    if a.gallery_index:
        entries = json.loads(a.gallery_index.read_text())
        shots = project / 'gallery'
        shots.mkdir(exist_ok=True)
        for entry in entries:
            open_file(a.gallery_index.parent / entry['bundle'])
            pixels = capture('gallery/' + entry['id'].replace('/', '-'))
            assert any(sum(rgb(pixels, px, py)) < 500
                       for py in range(48, min(280, pixels[1]), 4)
                       for px in range(24, min(280, pixels[0]), 4)), 'Empty Android example: ' + entry['id']
            print('Android rendered ' + entry['id'] + ': ' + entry['name'], flush=True)
        print(f'All {len(entries)} full gallery ZIBs rendered through the Android VM', flush=True)
finally:
    # A stopped emulator must not replace the original test failure with a
    # cleanup exception. Every command remains scoped to this test device.
    for arguments in (['shell', 'rm', '-f', '/data/local/tmp/kryon-zib-test.zib'],
                      ['shell', 'am', 'force-stop', package], ['uninstall', package]):
        try:
            subprocess.run([*command, *arguments], env=env, capture_output=True,
                           timeout=20, check=False)
        except (OSError, subprocess.TimeoutExpired):
            pass
