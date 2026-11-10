"""Real Android two-component fixture; use only an explicit disposable test device."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import time

from toolchain import UI_MODULE_ARGS, ROOT, ZIRAN, ZIRAN_ROOT
from android_soft_keyboard import soft_keyboard_text
from android_png_probe import png_pixels, rgb

parser = argparse.ArgumentParser()
parser.add_argument('--serial', choices=['ZE2223BQZT'])
parser.add_argument('--build-only', action='store_true')
parser.add_argument('--skip-build', action='store_true')
a = parser.parse_args()
project = ROOT / 'build/scratch/android-components'
project.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(name, None)
env['YUE_DESKTOP_RECOVERY'] = '0'
env.setdefault('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))
package = 'com.kryon.componenttest'

def run(command, **kw):
    command = list(map(str, command))
    print('Android components: ' + shlex.join(command[:8]), flush=True)
    return subprocess.run(command, cwd=project, env=env, stdin=subprocess.DEVNULL,
                          timeout=kw.pop('timeout', 1800), check=True, **kw)

if not a.skip_build:
    (project / 'src').mkdir(exist_ok=True)
    (project / 'assets').mkdir(exist_ok=True)
    (project / 'ziran.toml').write_text('''[package]
name = "android_components"
entry = "src/main.zi"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
default_profile = "android"
[tool.kryon.profiles.android]
backend = "android"
codegen = "c99"
assets = "assets"
[tool.kryon.android]
package = "com.kryon.componenttest"
abis = "armeabi-v7a"
min_sdk = "21"
''')
    (project / 'ziran.local.toml').write_text(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    run([ZIRAN, 'lock'])
    fixture = ROOT / 'tests/android_components'
    for name, defines in [('a-v1', []), ('b-v1', ['ANDROID_COMPONENT_B']),
                          ('a-v2', ['ANDROID_COMPONENT_V2']),
                          ('fail-v2', ['ANDROID_COMPONENT_V2', 'ANDROID_COMPONENT_FAIL'])]:
        run([ZIRAN, 'bundle', '--root', fixture, '--module-path', ROOT / 'src/backend',
             *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std',
             '--define', 'KRYON_HOSTED_UI',
             *[part for define in defines for part in ('--define', define)],
             '--entry', 'component:Frame', '-o', project / 'assets' / (name + '.zx'), fixture / 'component.zi'])
    source = ROOT / 'tests/fixtures/component_bad_schema/component.zi'
    run([ZIRAN, 'bundle', '--root', source.parent, '--module-path', ROOT / 'src/backend',
         *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std',
         '--define', 'KRYON_HOSTED_UI', '--entry', 'component:Frame',
         '-o', project / 'assets/bad-schema.zx', source])
    run(['python3', ROOT / 'scripts/android_scaffold.py', '--root', project,
         '--name', 'Android components test', '--package', package, '--activity', 'MainActivity', '--kryon', ROOT])
    (project / 'src/component_test_host.zi').unlink(missing_ok=True)
    (project / 'src/component_test_codecs.zi').unlink(missing_ok=True)
    (project / 'src/main.zi').write_text(f'''#program_export
main :: () -> s32 {{ return 0 }}
''')
    cmake = project / 'droid/app/src/main/cpp/CMakeLists.txt'
    content = cmake.read_text()
    content = re.sub(r' --host "[^"]*"', '', content)
    content = content.replace('--codegen "${APP_CODEGEN}"', f'--codegen "${{APP_CODEGEN}}" --host "{ROOT / "src/backend/android_component_test_host.zi"}"')
    cmake.write_text(content)
    run([ZIRAN, 'tool', 'kryon', 'build', '--profile', 'android'])

apk = project / 'droid/app/build/outputs/apk/debug/app-debug.apk'
assert apk.is_file()
if a.build_only or not a.serial:
    print('Android component fixture APK built', flush=True)
    raise SystemExit(0)

adb = [Path(env['ANDROID_HOME']) / 'platform-tools/adb', '-s', a.serial]
def device(*arguments, timeout=30):
    return run([*adb, *arguments], timeout=timeout, capture_output=True).stdout

device('install', '-r', apk, timeout=90)
device('shell', 'run-as', package, 'rm', '-f', 'files/component-command')
device('shell', 'am', 'start', '-W', '-n', package + '/.MainActivity')
pid = device('shell', 'pidof', package).decode().strip()
assert pid.isdecimal()
def logs():
    return device('shell', 'logcat', '-d', '--pid=' + pid, '-v', 'brief', '-t', '2000').decode(errors='replace')
def wait_for(text, since=0):
    deadline = time.monotonic() + 25
    while True:
        log = logs()
        assert 'FAILED' not in log and 'invariant failed' not in log and 'isolation failed' not in log, log[-4000:]
        if log.count(text) > since:
            return
        assert time.monotonic() < deadline, 'Missing ' + text + '\n' + log[-4000:]
        time.sleep(.2)
def capture(name):
    focused = device('shell', 'dumpsys', 'activity', 'activities')
    assert any(package.encode() in line for line in focused.splitlines()
               if b'mResumedActivity' in line or b'topResumedActivity' in line), 'Component test app is not foreground'
    path = project / (name + '.png')
    path.write_bytes(device('exec-out', 'screencap', '-p'))
    return png_pixels(path)
def tap(x, y):
    device('shell', 'input', 'tap', str(round(x * scale + left)), str(round(y * scale + top)))
def control(command, expected):
    previous = logs().count('component B pixels, state and handle unchanged')
    device('shell', shlex.join(['run-as', package, 'sh', '-c', 'printf ' + command + ' > files/component-command']))
    wait_for(expected)
    wait_for('component B pixels, state and handle unchanged', previous)

completed = False
try:
    wait_for('component state A 0')
    wait_for('component state B 0')
    initial = capture('initial')
    density = device('shell', 'wm', 'density').decode()
    scale = int(re.findall(r'(?:Physical|Override) density: (\d+)', density)[-1]) / 160
    red = [(x, y) for y in range(initial[1]) for x in range(initial[0]) if rgb(initial, x, y) == (230, 30, 40)]
    assert red, 'Native component marker did not render'
    left = min(x for x, y in red); top = min(y for x, y in red)
    tap(100, 38); wait_for('component state A 1')
    tap(100, 134)
    soft_keyboard_text(device, 'alpha')
    wait_for('component state A 1 alpha')
    device('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    tap(100, 218); wait_for('component state B 1')
    tap(100, 314)
    soft_keyboard_text(device, 'beta')
    wait_for('component state B 1 beta')
    device('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    capture('before-replacement')
    control('1', 'component replacement accepted; stale handle rejected')
    wait_for('component state A 1 alpha v2')
    capture('replaced')
    control('2', 'incompatible component rejected atomically')
    control('3', 'failing component rejected atomically')
    tap(100, 38); wait_for('component state A 3 alpha v2')
    tap(100, 218); wait_for('component state B 2 beta')
    for name in ('resumed', 'resumed-again'):
        previous = logs().count('component surface ready')
        a_reports = logs().count('component surface state A 3 alpha v2')
        b_reports = logs().count('component surface state B 2 beta')
        device('shell', 'input', 'keyevent', 'KEYCODE_HOME')
        time.sleep(.5)
        device('shell', 'am', 'start', '-W', '-n', package + '/.MainActivity')
        wait_for('component surface ready', previous)
        assert device('shell', 'pidof', package).decode().strip() == pid, 'Native host restarted'
        wait_for('component surface state A 3 alpha v2', a_reports)
        wait_for('component surface state B 2 beta', b_reports)
        capture(name)
    print('Android real touch/text, nested-state migration, stale-handle rejection, B isolation, atomic rollback and surface recreation passed', flush=True)
    completed = True
finally:
    if completed:
        device('shell', 'am', 'force-stop', package)
        device('uninstall', package)
    else:
        print('Android component failure: installed fixture and captures retained', flush=True)
