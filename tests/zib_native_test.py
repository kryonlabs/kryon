"""Exercise the native renderer with a downloaded ZIB on its own display."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import time

from toolchain import ROOT
from raylib_project_test import png_pixels, rgb

p = argparse.ArgumentParser()
p.add_argument('--player', type=Path, required=True)
p.add_argument('--bundle', type=Path, required=True)
p.add_argument('--runner', default='')
p.add_argument('--unicode-helper', type=Path)
p.add_argument('--private', action='store_true')
a = p.parse_args()
output = ROOT / 'build/zib-native-test'
output.mkdir(parents=True, exist_ok=True)
if not a.private:
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    env.pop('KRYON_FONT_PATH', None)
    env['WINEPREFIX'] = str(output / 'wine')
    env['WINEDEBUG'] = '-all'
    subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1280x900x24', sys.executable,
                    __file__, *sys.argv[1:], '--private'], env=env, check=True, timeout=300)
    raise SystemExit(0)

assert os.environ.get('DISPLAY'), 'Run through the private-display parent'
for mode in ('keyboard', 'unicode'):
    if a.runner == 'wine' and mode == 'unicode' and not a.unicode_helper:
        subprocess.run(['setxkbmap', 'fr'], check=True)
    bundle = str(a.bundle.resolve())
    if a.runner == 'wine':
        bundle = 'Z:' + bundle.replace('/', '\\')
    command = ([a.runner] if a.runner else []) + [str(a.player.resolve()), bundle]
    with (output / f'{a.runner or "native"}-{mode}.log').open('wb') as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            window = None
            while time.monotonic() < deadline and process.poll() is None:
                # This server belongs only to this player and its Wine process
                # family. It cannot see or target the owner's desktop.
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', '^Kryon ZIB$'],
                                       capture_output=True, text=True)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[0]
                    break
                time.sleep(.1)
            assert window is not None, 'native player did not open'
            subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)
            time.sleep(2)
            shot = output / f'{a.runner or "native"}-{mode}.png'
            subprocess.run(['import', '-window', window, 'PNG32:' + str(shot)], check=True)
            png = png_pixels(shot)
            assert rgb(png, 550, 130) == (230, 30, 40), 'embedded image missing'
            assert rgb(png, 10, 10) == (248, 250, 252), 'canvas background missing'
            assert any(sum(rgb(png, x, y)) < 400 for y in range(128, 165)
                       for x in range(40, 350)), 'text missing'
            subprocess.run(['xdotool', 'windowsize', '--sync', window, '1100', '700'], check=True)
            subprocess.run(['xdotool', 'mousemove', '--window', window, '100', '70', 'mousedown', '1'], check=True)
            time.sleep(.3)
            subprocess.run(['xdotool', 'mouseup', '1'], check=True)
            time.sleep(.3)
            if mode == 'unicode' and a.runner != 'wine':
                subprocess.run(['xmodmap', '-e', 'keycode 200 = eacute'], check=True)
            if mode == 'unicode' and a.unicode_helper:
                subprocess.run([a.runner, str(a.unicode_helper.resolve())], check=True, timeout=30)
            else:
                subprocess.run(['xdotool', 'key', 'eacute' if mode == 'unicode' else 'a'], check=True)
            assert process.wait(timeout=40) == 0, 'native player failed'
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    contents = (output / f'{a.runner or "native"}-{mode}.log').read_text(errors='replace')
    assert 'zib interaction passed' in contents, contents[-3000:]
    assert 'interaction failed' not in contents, contents[-3000:]
print('Native ZIB image, text, resize, click, keyboard and Unicode passed')
