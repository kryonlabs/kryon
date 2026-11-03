#!/usr/bin/env python3
"""SDL controller mapping and own-window navigation with private virtual devices."""
import fcntl
import os
from pathlib import Path
import shlex
import subprocess
from toolchain import ZIRAN_ROOT, ZIRAN

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/desktop-gamepad'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH', 'KRYON_TEST_DRIVER'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV['TMPDIR'] = str(WORK)
ENV['SDL_VIDEODRIVER'] = 'x11'
ENV['SDL_RENDER_DRIVER'] = 'software'
# Only the process-local virtual controllers (vendor/product zero) are eligible.
ENV['SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT'] = '0x0000/0x0000'
ENV['SDL_JOYSTICK_HIDAPI'] = '0'
ENV['ZIRAN_JOBS'] = '2'

def run(args):
    print(shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)

def main():
    run(['git', 'check-ignore', WORK / 'test'])
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        common = ['--root', ROOT / 'tests', '--module-path', ROOT / 'src/backend',
                  '--module-path', ROOT / 'src/ui', '--module-path', ZIRAN_ROOT / 'std']
        libs = subprocess.check_output(['pkg-config', '--libs', 'sdl2', 'cairo', 'freetype2',
                                       'fontconfig', 'pangocairo', 'gio-2.0'], text=True).split()
        for name in ('desktop_gamepad_mapping_test', 'desktop_keys_test', 'desktop_gamepad_test'):
            source = ROOT / 'tests' / (name + '.zi')
            ir = WORK / (name + '-ir')
            run([ZIRAN, 'ir', *common, '-o', ir, source])
            for form in ('source', 'saved'):
                paths = common if form == 'source' else ['--root', ir]
                entry = source if form == 'source' else ir / (name + '.zir')
                gen = WORK / (name + '-c')
                run([ZIRAN, 'build', '--target=c', *paths, '--entry', name + ':main', '-o', gen, entry])
                binary = gen / 'test'
                run([os.environ.get('CC', 'cc'), '-O1', '-I' + str(ZIRAN_ROOT / 'include'),
                     '-I' + str(gen), *sorted(gen.glob('*.c')), *libs, '-lm', '-o', binary])
                if name == 'desktop_gamepad_test':
                    run(['timeout', '--kill-after=2s', '45s', 'xvfb-run', '-a', '-n', '390',
                         '-s', '-screen 0 640x480x24 -nolisten tcp', binary])
                else:
                    run([binary])
                    go = WORK / (name + '-go')
                    run([ZIRAN, 'build', '--target=go', '--pkg', 'main', '--exe', *paths,
                         '--entry', name + ':main', '-o', go, entry])
                    run(['go', 'build', '-o', go / 'test', *sorted(go.glob('*.go'))])
                    run([go / 'test'])
                    bundle = WORK / (name + '.zx')
                    run([ZIRAN, 'bundle', *paths, '--entry', name + ':main', '-o', bundle, entry])
                    result = subprocess.check_output([str(ZIRAN), 'run', str(bundle)], cwd=ROOT, env=ENV, text=True).strip()
                    if result != '0':
                        raise RuntimeError('portable mapping failed: ' + result)
        run(['make', 'desktop-frame-input-test'])
        print('PASS: source/saved SDL virtual hotplug, bounded devices, navigation, held sources, focus and teardown; native/portable mappings')

if __name__ == '__main__':
    main()
