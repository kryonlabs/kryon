#!/usr/bin/env python3
"""XDND drag source on a private Xvfb: a real drop onto an XDND target window.

Builds tests/drag_source_probe.zi from source and saved IR. Each probe starts a
drag while the private pointer's button is held; xdotool moves the pointer onto
tests/xdnd_target.py and releases it. The target must receive the exact
text/uri-list and the probe must report a finished copy. A second drag released
over the bare root window must cancel. Never runs on the inherited display.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/drag-source-linux-test'
SCRUB = {'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
         'SESSION_MANAGER', 'ICEAUTHORITY'}
STATUS = {0: 'Idle', 1: 'Dragging', 2: 'Local', 3: 'Dropped', 4: 'Finished', 5: 'Cancelled', 6: 'Failed'}


def run(args, **kwargs):
    subprocess.run(list(map(str, args)), check=True, **kwargs)


def xdotool(*args):
    run(['xdotool', *args])


def drag(probe, end, target_output=None):
    xdotool('mousemove', '--sync', '40', '40')
    xdotool('mousedown', '1')
    process = subprocess.Popen([probe], stdout=subprocess.PIPE, text=True)
    try:
        assert process.stdout.readline().strip() == 'ready'
        x, y = 40, 40
        for step in range(1, 21):
            xdotool('mousemove', '--sync', str(40 + (end[0] - 40) * step // 20),
                    str(40 + (end[1] - 40) * step // 20))
            time.sleep(.03)
        time.sleep(.3)
    finally:
        xdotool('mouseup', '1')
    output, _ = process.communicate(timeout=30)
    assert process.returncode == 0, process.returncode
    fields = list(map(int, output.split()))
    return {'status': STATUS[fields[0]], 'performed': fields[1], 'success': bool(fields[2]),
            'local': bool(fields[3]), 'accepted': bool(fields[4])}


def execute(probe):
    output = WORK / 'target.json'
    output.unlink(missing_ok=True)
    target = subprocess.Popen([sys.executable, ROOT / 'tests/xdnd_target.py', '300', '200', '240', '180',
                               output], stdout=subprocess.PIPE, text=True)
    assert target.stdout.readline().strip() == 'ready'
    report = drag(probe, (420, 290))
    target.wait(timeout=30)
    received = json.loads(output.read_text())
    assert report == {'status': 'Finished', 'performed': 1, 'success': True, 'local': False,
                      'accepted': True}, report
    assert received['data'] == 'file:///tmp/kryon%20drag/a.txt\r\n', received
    assert received['types'] == ['text/uri-list', 'text/plain;charset=utf-8'], received
    assert received['targets'] == ['TARGETS', 'text/uri-list', 'text/plain;charset=utf-8'], received
    assert received['actions'] and set(received['actions']) == {'XdndActionCopy'}, received
    assert received['drops'] == 1 and received['positions'] >= 1, received
    report = drag(probe, (700, 450))
    assert report['status'] == 'Cancelled' and not report['accepted'], report


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--execute':
        execute(Path(sys.argv[2])); return
    from toolchain import ZIRAN, ZIRAN_ROOT
    WORK.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k not in SCRUB}
    # python-xlib for the XDND target stays importable with a private HOME.
    import site
    env.setdefault('PYTHONUSERBASE', site.getuserbase())
    for key, folder in [('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data'),
                        ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime')]:
        path = WORK / folder; path.mkdir(mode=0o700, exist_ok=True); path.chmod(0o700)
        env[key] = str(path)
    env.update(HARMONY_DESKTOP_RECOVERY='0', YUE_DESKTOP_RECOVERY='0', LDLIBS='-lxcb', TMPDIR=str(WORK))
    source = ROOT / 'tests/drag_source_probe.zi'
    ir = WORK / 'ir'
    common = ['--module-path', ROOT / 'src/backend', '--module-path', ZIRAN_ROOT / 'std']
    run([ZIRAN, 'ir', '--root', ROOT / 'tests', *common, '-o', ir, source], env=env)
    for form in ('source', 'saved'):
        root, input_ = (ROOT / 'tests', source) if form == 'source' else (ir, ir / 'drag_source_probe.zir')
        output = WORK / 'probe'
        run([ZIRAN, 'build', '--target=c', '--exe', '--root', root,
             '--entry', 'drag_source_probe:main', *([] if form == 'saved' else common),
             '-o', output, input_], env=env)
        probe = output / 'drag_source_probe'
        run(['xvfb-run', '-a', '-s', '-screen 0 1024x768x24 -nolisten tcp',
             'dbus-run-session', '--', sys.executable, __file__, '--execute', probe], env=env)
        print(f'XDND drag source {form}: PASS', flush=True)


if __name__ == '__main__':
    main()
