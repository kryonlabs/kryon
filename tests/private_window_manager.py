"""Run one native GUI test with its own WM on an already private Xvfb display."""
import ctypes as c
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

display = os.environ.get('DISPLAY', '')
assert display.startswith(':') and int(display[1:].split('.')[0]) >= 200
assert 'xvfb-run.' in os.environ.get('XAUTHORITY', '')
assert os.environ.get('YUE_DESKTOP_RECOVERY') == '0'
x11 = c.CDLL('libX11.so.6')
x11.XOpenDisplay.argtypes, x11.XOpenDisplay.restype = [c.c_char_p], c.c_void_p
x11.XInternAtom.argtypes, x11.XInternAtom.restype = [c.c_void_p, c.c_char_p, c.c_int], c.c_ulong
x11.XGetSelectionOwner.argtypes, x11.XGetSelectionOwner.restype = [c.c_void_p, c.c_ulong], c.c_ulong
x11.XCloseDisplay.argtypes = [c.c_void_p]
connection = x11.XOpenDisplay(None)
assert connection
with tempfile.TemporaryDirectory(prefix='private-wm-', dir=os.environ['TMPDIR']) as scratch:
    env = dict(os.environ, HOME=scratch, XDG_CONFIG_HOME=scratch, XDG_CACHE_HOME=scratch,
               XDG_RUNTIME_DIR=scratch, NO_AT_BRIDGE='1', GSETTINGS_BACKEND='memory')
    wm = subprocess.Popen(['xfwm4', '--compositor=off'], env=env)
    try:
        selection = x11.XInternAtom(connection, b'WM_S0', 0)
        deadline = time.monotonic() + 5
        while not x11.XGetSelectionOwner(connection, selection) and time.monotonic() < deadline:
            assert wm.poll() is None, 'private WM exited'
            time.sleep(.02)
        assert x11.XGetSelectionOwner(connection, selection), 'private WM did not claim screen'
        code = subprocess.run(sys.argv[1:], env=env, timeout=60).returncode
    finally:
        wm.terminate()
        try:
            wm.wait(timeout=3)
        except subprocess.TimeoutExpired:
            wm.kill()
            wm.wait(timeout=3)
        x11.XCloseDisplay(connection)
raise SystemExit(code)
