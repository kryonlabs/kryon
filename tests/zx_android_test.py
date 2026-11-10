"""Build a generic Android player and exercise an explicitly selected test device."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import time
import tempfile
import zipfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from android_soft_keyboard import soft_keyboard_text, wait_keyboard_visible
from android_png_probe import png_pixels, rgb, solid_png

p = argparse.ArgumentParser()
target = p.add_mutually_exclusive_group()
target.add_argument('--serial', help='A disposable emulator-* device or the USB Moto (ZE2223BQZT)')
target.add_argument('--build-only', action='store_true', help='Build and inspect the APK; never invoke adb')
p.add_argument('--skip-build', action='store_true')
p.add_argument('--gallery-index', type=Path)
a = p.parse_args()
project = ROOT / 'build/zx-android-test'
project.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(name, None)
env['YUE_DESKTOP_RECOVERY'] = '0'
env.setdefault('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))


def run(command, **kw):
    command = list(map(str, command))
    device_step = Path(command[0]).name == 'adb'
    if device_step:
        print('Android device step: ' + shlex.join(command[3:]), flush=True)
    timeout = kw.pop('timeout', 90 if device_step and 'install' in command else 30 if device_step else 1800)
    try:
        return subprocess.run(command, cwd=project, env=env, check=True,
                              timeout=timeout, stdin=subprocess.DEVNULL, **kw)
    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
        for data in (getattr(error, 'stdout', None), getattr(error, 'stderr', None)):
            if data:
                print(data[-4000:].decode(errors='replace') if isinstance(data, bytes) else data[-4000:], flush=True)
        raise


if not a.skip_build:
    # Only these test-owned scaffold outputs are regenerated; cached ABI
    # objects stay in place and no application source checkout is touched.
    for relative in ('droid/app/build.gradle', 'droid/app/src/main/AndroidManifest.xml',
                     'droid/app/src/main/cpp/CMakeLists.txt',
                     'droid/app/src/main/java/com/kryon/zxtest/MainActivity.java'):
        (project / relative).unlink(missing_ok=True)
    (project / 'src').mkdir(exist_ok=True)
    (project / 'assets').mkdir(exist_ok=True)
    (project / 'src/app.zi').write_bytes((ROOT / 'tests/zx/mobile_app.zi').read_bytes())
    solid_png(project / 'assets/red.png', 32, 32, (230, 30, 40, 255))
    (project / 'ziran.toml').write_text('''[package]
name = "zx_android_probe"
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
codegen = "zx"
assets = "assets"
[tool.kryon.profiles.android]
backend = "android"
codegen = "zx"
assets = "assets"
[tool.kryon.android]
package = "com.kryon.zxtest"
abis = "x86_64,arm64-v8a,armeabi-v7a"
min_sdk = "21"
''')
    (project / 'ziran.local.toml').write_text(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    run([ZIRAN, 'lock'])
    for profile in ('portable', 'android'):
        run([ZIRAN, 'tool', 'kryon', 'build', '--profile', profile])

portable = project / 'build/zx_android_probe-portable.zx'
android = project / 'build/zx_android_probe-android.zx'
if not portable.exists():
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', 'portable'])
assert portable.read_bytes() == android.read_bytes(), 'Android changed the guest bundle'
apk = project / 'droid/app/build/outputs/apk/debug/app-debug.apk'
with zipfile.ZipFile(apk) as archive:
    assert archive.read('assets/app.zx') == portable.read_bytes()
    assert 'assets/kryon-font.ttf' not in archive.namelist()
    for abi in ('arm64-v8a', 'armeabi-v7a', 'x86_64'):
        assert f'lib/{abi}/libmain.so' in archive.namelist(), abi
    assert not any('raylib' in name.lower() for name in archive.namelist())
    ndk = Path(os.environ.get('ANDROID_NDK', env['ANDROID_HOME'] + '/ndk/28.2.13676358'))
    readelf = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
    assert readelf.is_file(), 'Android NDK readelf is required for link proof'
    with tempfile.TemporaryDirectory(dir=project / 'build') as temporary:
        for name in archive.namelist():
            if name.startswith('lib/') and name.endswith('.so'):
                library = archive.extract(name, temporary)
                dynamic = subprocess.check_output(
                    [readelf, '-d', library], text=True)
                assert 'libraylib' not in dynamic, name
                assert 'libEGL' not in dynamic and 'libGLESv2' not in dynamic, name
                assert 'libOpenSLES' not in dynamic, name
generated = list((project / 'droid/app/.cxx').rglob('ziran/generated/*_app.c'))
# The generic BundleApp host is native; the selected product app is not.
assert all(re.fullmatch(r'(?:p[0-9a-f]{16}_)?(?:bundle_app|hosted_app)\.c', path.name)
           for path in generated), 'The APK compiled the guest app natively'
print('Android three-ABI generic APK, embedded assets and identical desktop ZX passed', flush=True)
if a.build_only or not a.serial:
    raise SystemExit(0)
is_emulator = a.serial.startswith('emulator-')
is_usb_moto = a.serial == 'ZE2223BQZT'
assert is_emulator or is_usb_moto, 'This test operates only a disposable emulator or the USB Moto test phone'
adb = Path(env['ANDROID_HOME']) / 'platform-tools/adb'
command = [adb, '-s', a.serial]
if is_emulator:
    assert run([*command, 'shell', 'getprop', 'ro.kernel.qemu'], capture_output=True).stdout.strip() == b'1'
package = 'com.kryon.zxtest'
completed = False
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
    def label_tail_ink(pixels):
        return sum(sum(rgb(pixels, px, py)) < 500
                   for py in range(round(120 * scale + top), round(160 * scale + top))
                   for px in range(round(160 * scale), round(280 * scale)))
    assert label_tail_ink(before) > 0, 'Initial label proof region is empty'
    assert label_tail_ink(clicked) == 0, 'Old label pixels survived the shorter replacement label'

    run([*command, 'shell', 'input', 'tap', str(round(100 * scale)), str(round(320 * scale + top))])
    time.sleep(1)
    def device(*arguments):
        return run([*command, *arguments], capture_output=True).stdout
    wait_keyboard_visible(device, True)
    device('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    wait_keyboard_visible(device, False)
    # An outside tap cannot reopen the dismissed input method. Re-tapping
    # the actual guest field can reopen it while the app remains alive.
    device('shell', 'input', 'tap', str(round(310 * scale)), str(round(170 * scale + top)))
    time.sleep(.3)
    wait_keyboard_visible(device, False)
    device('shell', 'input', 'tap', str(round(100 * scale)), str(round(320 * scale + top)))
    wait_keyboard_visible(device, True)
    capture('keyboard-retapped')
    soft_keyboard_text(device, 'portable')
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
    print('Android device ZX image, density, touch, exact text and pause/resume passed')
    def results():
        log = run([*command, 'logcat', '-d', '-v', 'brief', '-t', '2000', '-s', 'ZX:I'], capture_output=True).stdout
        return log.count(b'Bundle loaded'), log.count(b'Bundle rejected')

    run([*command, 'shell', 'run-as', package, 'mkdir', '-p', 'files'])
    private = run([*command, 'shell', 'run-as', package, 'pwd'], capture_output=True).stdout.decode().strip()
    transport = '/data/local/tmp/kryon-zx-test.zx'
    def open_file(bundle, accepted=True):
        previous = results()[0 if accepted else 1]
        run([*command, 'push', bundle, transport], capture_output=True)
        run([*command, 'shell', shlex.join(['run-as', package, 'sh', '-c',
             'cat /data/local/tmp/kryon-zx-test.zx > files/selected.zx'])])
        run([*command, 'shell', 'am', 'start', '-W', '-a', 'android.intent.action.VIEW',
             '-d', 'file://' + private + '/files/selected.zx', '-t', 'application/vnd.ziran.bundle',
             '-n', package + '/.Zx'], capture_output=True)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if results()[0 if accepted else 1] > previous:
                time.sleep(.4)
                return
            time.sleep(.2)
        raise AssertionError('Android did not accept/reject the selected bundle as expected')

    invalid = project / 'invalid.zx'
    invalid.write_bytes(b'invalid ZX')
    open_file(invalid, False)
    rejected = capture('invalid-file')
    assert rgb(rejected, x, y) == rgb(resumed_again, x, y), 'Invalid file destroyed app state'
    assert rgb(rejected, round(250 * scale), round(230 * scale + top)) == (230, 30, 40), 'Rejected file cleared retained image'
    open_file(portable)
    reopened = capture('file-opened')
    assert rgb(reopened, x, y) == rgb(before, x, y), 'A valid downloaded file did not start a fresh instance'
    assert rgb(reopened, round(250 * scale), round(230 * scale + top)) == (230, 30, 40), 'Accepted file lost candidate image'
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
        print(f'All {len(entries)} full gallery ZXs rendered through the Android VM', flush=True)
    completed = True
finally:
    if not completed:
        print('Android test failed; installed test state and captures retained for diagnosis', flush=True)
    # A stopped emulator must not replace the original test failure with a
    # cleanup exception. Every command remains scoped to this test device.
    for arguments in (() if not completed else (['shell', 'rm', '-f', '/data/local/tmp/kryon-zx-test.zx'],
                      ['shell', 'am', 'force-stop', package], ['uninstall', package])):
        try:
            subprocess.run([*command, *arguments], env=env, capture_output=True,
                           timeout=20, check=False)
        except (OSError, subprocess.TimeoutExpired):
            pass
