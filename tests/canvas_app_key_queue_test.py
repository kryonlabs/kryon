#!/usr/bin/env python3
"""Exercise the real Canvas key-delivery/frame lifecycle without a display."""
import fcntl
import os
from pathlib import Path
import subprocess

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/canvas-app-key-queue'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV['ZIRAN_JOBS'] = '1'

SOURCE = '''#import "page_core"
#import "canvas_events"
#import "page_input"
#import "canvas_input"

main :: () -> s32 {
    CanvasEventsReset()
    unused CanvasKeyDown("Home", "Home", false, false)
    unused CanvasKeyUp("Home", "Home")
    unused CanvasKeyDown("Digit4", "4", false, false)
    unused CanvasKeyUp("Digit4", "4")
    // CanvasAppMainLoop takes one AppKey before CanvasAppFrame begins drawing.
    first := CanvasAppKeyTake()
    CanvasEventsBeginFrame()
    if first != 268 { return 1 }
    if !IsKeyPressed(52) { return 2 }
    CanvasEventsEndFrame()
    if canvas_keys_pressed_count != 0 || canvas_keys_released_count != 0 { return 3 }
    second := CanvasAppKeyTake()
    CanvasEventsBeginFrame()
    if second != 52 { return 4 }
    // Delivering an unread event must not replay its pressed/released flags.
    if canvas_keys_pressed_count != 0 || canvas_keys_released_count != 0 { return 5 }
    CanvasEventsEndFrame()
    if CanvasAppKeyTake() != 0 { return 6 }
    // Events arriving while drawing yields survive cleanup, with no duplication.
    unused CanvasKeyDown("KeyA", "a", false, false)
    CanvasEventsBeginFrame()
    if CanvasAppKeyTake() != 65 { return 7 }
    unused CanvasKeyDown("KeyB", "b", false, false)
    CanvasEventsEndFrame()
    if !CanvasQueueHas(*canvas_keys_pressed[0], canvas_keys_pressed_count, 66) { return 8 }
    if CanvasAppKeyTake() != 66 { return 9 }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    if CanvasAppKeyTake() != 0 || canvas_keys_pressed_count != 0 { return 10 }
    unused CanvasKeyDown("KeyC", "c", false, false)
    if GetKeyPressed() != 67 || CanvasAppKeyTake() != 0 { return 16 }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    // Repeats also survive frames and retain the existing fresh-key priority.
    unused CanvasKeyDown("KeyA", "a", true, false)
    unused CanvasKeyDown("KeyB", "b", true, false)
    if CanvasAppKeyTake() != 65 { return 11 }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    if canvas_keys_repeated_count != 0 { return 12 }
    if CanvasAppKeyTake() != 66 || CanvasAppKeyTake() != 0 { return 13 }
    unused CanvasKeyDown("KeyC", "c", false, false)
    CanvasEventsReset()
    if CanvasAppKeyTake() != 0 { return 14 }
    // A focus change cancels pending shortcuts instead of replaying them later.
    unused CanvasKeyDown("KeyD", "d", false, false)
    unused CanvasEvent(CanvasEventBlur, 0, 0, 0.0, 0.0, null, null)
    if CanvasAppKeyTake() != 0 { return 15 }
    return 0
}
'''


def run(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)


def main():
    run(['git', 'check-ignore', 'work/canvas-app-key-queue/test'])
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        fixture = WORK / 'app_keys.zi'
        fixture.write_text(SOURCE)
        # No browser cursor/gamepad effects are needed by this native test.
        (WORK / 'page_input.zi').write_text('PageInputCursor :: (mode: s32) {}\nPageGamepadQuery :: (pad: s32, field: s32, index: s32) -> s32 { return 0 }\nPageGamepadAxis :: (pad: s32, axis: s32) -> float64 { return 0.0 }\n')
        paths = ['--root', WORK, '--module-path', ROOT / 'tests/page_fake',
                 '--module-path', ROOT / 'src/backend', '--module-path', ROOT / 'src/ui',
                 '--module-path', ZIRAN_ROOT / 'std']
        define = ['--define', 'PLATFORM_WEB']
        ir = WORK / 'ir'
        run([ZIRAN, 'ir', *paths, *define, '-o', ir, fixture])
        for form in ('source', 'saved'):
            source = fixture if form == 'source' else ir / 'app_keys.zir'
            modules = paths if form == 'source' else ['--root', ir]
            run([ZIRAN, 'build', '--target=c', '--exe', '--entry', 'app_keys:main',
                 *modules, *define, '-o', WORK / 'native', source])
            run([WORK / 'native/app_keys'])
            print(f'{form}: native Canvas AppKey delivery passed', flush=True)


if __name__ == '__main__':
    main()
