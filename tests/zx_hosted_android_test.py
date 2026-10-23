"""Hosted app persistence on the explicitly selected, disposable Kryon AVD."""
import argparse
import os
from pathlib import Path
import subprocess
import time

from toolchain import ROOT
from raylib_project_test import png_pixels, rgb

parser = argparse.ArgumentParser()
parser.add_argument('--serial', required=True)
args = parser.parse_args()
assert args.serial.startswith('emulator-'), 'Physical devices are never test targets'
adb = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))) / 'platform-tools/adb'
output = ROOT / 'build/scratch/zx-hosted'
package = 'com.kryon.zxtest'


def run(*arguments, **kwargs):
    return subprocess.run([str(adb), '-s', args.serial, *map(str, arguments)],
                          check=True, capture_output=True, timeout=40, **kwargs)


assert run('shell', 'getprop', 'ro.kernel.qemu').stdout.strip() == b'1'
assert run('emu', 'avd', 'name').stdout.splitlines()[0].strip() == b'KryonZx'
apk = ROOT / 'build/zx-android-test/droid/app/build/outputs/apk/debug/app-debug.apk'
run('install', '-r', apk)
try:
    run('shell', 'wm', 'size', '960x600')
    run('shell', 'wm', 'density', '160')
    run('shell', 'am', 'start', '-W', '-n', package + '/.MainActivity')
    for name in ('hosted', 'hosted_other'):
        run('push', output / f'{name}.zx', f'/data/local/tmp/{name}.zx')

    def open_app(name):
        run('shell', 'am', 'start', '-W', '-n', package + '/.Zx',
            '-a', 'android.intent.action.VIEW', '-t', 'application/x-zx',
            '-d', f'file:///data/local/tmp/{name}.zx')
        time.sleep(1)

    def capture(name):
        foreground = run('shell', 'dumpsys', 'activity', 'activities').stdout
        assert any(package.encode() in line for line in foreground.splitlines()
                   if b'mResumedActivity' in line or b'topResumedActivity' in line)
        path = output / f'android-{name}.png'
        path.write_bytes(run('exec-out', 'screencap', '-p').stdout)
        pixels = png_pixels(path)
        assert rgb(pixels, 550, 130) == (230, 30, 40), 'Hosted embedded image missing'
        return b''.join(bytes(rgb(pixels, x, y)) for y in range(128, 168) for x in range(40, 340))

    open_app('hosted')
    original = capture('initial')
    run('shell', 'input', 'tap', '100', '70')
    time.sleep(.4)
    clicked = capture('clicked')
    assert clicked != original, 'Hosted button did not change app state'
    open_app('hosted')
    assert capture('reopened') == clicked, 'Replacing the same app lost its saved document'
    # Back closes this test activity normally, exercising APP_SAVE before
    # stopping its disposable process and starting a fresh one.
    run('shell', 'input', 'keyevent', '4')
    time.sleep(1)
    run('shell', 'am', 'force-stop', package)
    run('shell', 'am', 'start', '-W', '-n', package + '/.MainActivity')
    open_app('hosted')
    assert capture('restored') == clicked, 'Android restart lost the document'
    open_app('hosted_other')
    assert capture('isolated') == original, 'Different app inherited the document'
    open_app('hosted')
    assert capture('returned') == clicked, 'Switching back lost the document'
finally:
    run('shell', 'wm', 'size', 'reset')
    run('shell', 'wm', 'density', 'reset')
    run('shell', 'am', 'force-stop', package)
    run('uninstall', package)
print('Android hosted ZX images, widgets, same-app replacement, close/save, cold restoration and app isolation passed')
