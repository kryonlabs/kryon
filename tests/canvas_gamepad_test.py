#!/usr/bin/env python3
"""Run the real Canvas input adapter and application queue against injected page gamepads."""
import fcntl
import os
from pathlib import Path
import shlex
import subprocess
from toolchain import UI_MODULE_ARGS, ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/canvas-gamepad'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV['TMPDIR'] = str(WORK)
ENV['ZIRAN_JOBS'] = '2'

FAKE = '''available: [5]bool;
buttons: [5][18]bool;
axes: [5][2]float64;
max_pad: s32;
PageInputCursor :: (mode: s32) {}
PageGamepadQuery :: (pad: s32, field: s32, index: s32) -> s32 {
    if pad > max_pad { max_pad = pad }
    if pad < 0 || pad >= 5 || !available[pad] { return 0 }
    if field == 0 { return 1 }
    if field == 2 && index >= 0 && index < 18 && buttons[pad][index] { return 1 }
    return 0
}
PageGamepadAxis :: (pad: s32, axis: s32) -> float64 {
    if pad < 0 || pad >= 5 || axis < 0 || axis > 1 { return 0.0 }
    return axes[pad][axis]
}
'''

def run(args):
    print(shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)

def main():
    run(['git', 'check-ignore', WORK / 'test'])
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        (WORK / 'page_input.zi').write_text(FAKE)
        source = WORK / 'canvas_gamepad_test.zi'
        source.write_text((ROOT / 'tests/canvas_gamepad_test.zi').read_text())
        paths = ['--root', WORK, '--module-path', WORK,
                 '--module-path', ROOT / 'tests/page_fake', '--module-path', ROOT / 'src/backend',
                 *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std']
        target = ['--target-os', 'web', '--target-cpu', 'wasm32']
        define = ['--define', 'PLATFORM_WEB']
        ir = WORK / 'ir'
        run([ZIRAN, 'ir', *paths, *target, *define, '-o', ir, source])
        for form in ('source', 'saved'):
            entry = source if form == 'source' else ir / 'canvas_gamepad_test.zir'
            modules = paths if form == 'source' else ['--root', ir]
            run([ZIRAN, 'build', '--target=c', '--exe', '--entry', 'canvas_gamepad_test:main',
                 *modules, *target, *define, '-o', WORK / 'native', entry])
            run([WORK / 'native/canvas_gamepad_test'])
        # Link the real CanvasPressedKey entry against its production module
        # graph as native code too; do not mistake this for WASM execution.
        host_paths = ['--root', ROOT / 'tests', '--module-path', ROOT / 'src/backend',
                      *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std']
        host_source = ROOT / 'tests/canvas_gamepad_host_test.zi'
        host_ir = WORK / 'host-ir'
        run([ZIRAN, 'ir', *host_paths, *target, *define, '-o', host_ir, host_source])
        for form in ('source', 'saved'):
            entry = host_source if form == 'source' else host_ir / 'canvas_gamepad_host_test.zir'
            modules = host_paths if form == 'source' else ['--root', host_ir]
            run([ZIRAN, 'build', '--target=c', '--exe', '--entry', 'canvas_gamepad_host_test:main',
                 *modules, *target, *define, '-o', WORK / 'host-native', entry])
            run([WORK / 'host-native/canvas_gamepad_host_test'])
        # The existing queue and event suite exercises keyboard/focus behavior
        # beside this fallback, using its established test-owned page provider.
        run(['python3', ROOT / 'tests/canvas_app_key_queue_test.py'])
        run(['sh', ROOT / 'tests/canvas_events_zi_test.sh'])
        print('PASS: Canvas source/saved native page adapter, navigation/axes, four pads, queue priority, focus/reset; keyboard/event regressions')
        print('Browser/WASM execution is not performed by this native page-provider test.')

if __name__ == '__main__':
    main()
