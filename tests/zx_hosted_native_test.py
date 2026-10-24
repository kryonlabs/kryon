"""Exercise only a hosted app's own windows on a private Xvfb server."""
import ctypes
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil
import time

from toolchain import ROOT
from raylib_project_test import png_pixels, rgb

parser = argparse.ArgumentParser()
parser.add_argument('--runner', choices=['wine'])
parser.add_argument('--player', type=Path, default=ROOT / 'build/zx-raylib/zx')
parser.add_argument('--private', action='store_true')
args = parser.parse_args()

output = ROOT / 'build/scratch/zx-hosted'
if not args.private:
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    subprocess.run(['xvfb-run', '-a', sys.executable, __file__, *sys.argv[1:], '--private'], env=env, check=True, timeout=120)
    raise SystemExit(0)


def close_owned_window(window):
    class Data(ctypes.Union):
        _fields_ = [('values', ctypes.c_long * 5), ('pad', ctypes.c_char * 20)]

    class Message(ctypes.Structure):
        _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong), ('send_event', ctypes.c_int),
                    ('display', ctypes.c_void_p), ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong),
                    ('format', ctypes.c_int), ('data', Data)]

    xlib = ctypes.CDLL('libX11.so.6')
    xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
    xlib.XOpenDisplay.restype = ctypes.c_void_p
    xlib.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    xlib.XInternAtom.restype = ctypes.c_ulong
    xlib.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.c_void_p]
    xlib.XFlush.argtypes = [ctypes.c_void_p]
    xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = xlib.XOpenDisplay(os.environ['DISPLAY'].encode())
    assert display
    try:
        event = Message(type=33, send_event=1, display=display, window=int(window), format=32)
        event.message_type = xlib.XInternAtom(display, b'WM_PROTOCOLS', 0)
        event.data.values[0] = xlib.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
        xlib.XSendEvent(display, int(window), 0, 0, ctypes.byref(event))
        xlib.XFlush(display)
    finally:
        xlib.XCloseDisplay(display)


with tempfile.TemporaryDirectory(prefix='native-data-', dir=output) as data:
    env = {**os.environ, 'KRYON_APP_DATA': data}
    storage = Path(data)
    if args.runner:
        # Use a real Windows path. Wine marks the Linux storage mount behind
        # Z: as a reparse point, which private AppFiles correctly refuses.
        storage = output / 'wine/drive_c/kryon-test-data' / Path(data).name
        storage.mkdir(parents=True, exist_ok=True)
        env['KRYON_APP_DATA'] = 'C:/kryon-test-data/' + Path(data).name
        env['WINEPREFIX'] = str(output / 'wine')
        env['WINEDEBUG'] = '-all'
    for attempt in range(2):
        log_path = output / f'native-{attempt}.log'
        with log_path.open('w') as log:
            bundle = str(output / 'hosted.zx')
            if args.runner: bundle = 'Z:' + bundle.replace('/', '\\')
            command = ([args.runner] if args.runner else []) + [str(args.player), bundle]
            app = subprocess.Popen(command,
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                window = None
                deadline = time.monotonic() + 30
                while time.monotonic() < deadline and app.poll() is None:
                    selector = ['--name', '^hosted$'] if args.runner else ['--pid', str(app.pid)]
                    found = subprocess.run(['xdotool', 'search', '--onlyvisible', *selector],
                                           capture_output=True, text=True)
                    if found.returncode == 0 and found.stdout.strip():
                        window = found.stdout.splitlines()[0]
                        break
                    time.sleep(.1)
                assert window, log_path.read_text()
                subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True, timeout=5)
                time.sleep(.5)
                shot = output / f'native-{attempt}.png'
                assert app.poll() is None, log_path.read_text()
                subprocess.run(['import', '-window', window, 'PNG32:' + str(shot)], check=True, timeout=5)
                assert rgb(png_pixels(shot), 550, 130) == (230, 30, 40), 'Hosted image missing'
                if attempt == 0:
                    subprocess.run(['xdotool', 'mousemove', '--window', window, '100', '70', 'mousedown', '1'], check=True)
                    time.sleep(.15)
                    subprocess.run(['xdotool', 'mouseup', '1'], check=True)
                    time.sleep(.3)
                close_owned_window(window)
                assert app.wait(timeout=15) == 0, log_path.read_text()
            finally:
                if app.poll() is None:
                    app.terminate()
                    app.wait(timeout=10)
        assert (storage / 'portable.probe/state.txt').read_text() == 'clicked', log_path.read_text()
        if attempt == 1:
            assert 'Hosted document restored' in log_path.read_text(), log_path.read_text()
    if args.runner:
        shutil.rmtree(storage)
print('Native hosted ZX images, widgets, close/save and restart restoration passed')
