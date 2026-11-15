#!/usr/bin/env python3
"""Exercise the real Canvas key-delivery/frame lifecycle without a display."""
import fcntl
import os
from pathlib import Path
import subprocess

from toolchain import UI_MODULE_ARGS, ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/canvas-app-key-queue'
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
#import "input_delivery"
#import "input_take"
#import "key_input"
#import "tree_input"
#import "session"

app_session: Session;

Take :: () -> (key: s32, modifiers: s32) {
    key, modifiers := InputDeliveryTake(*canvas_input, app_session, CanvasHeldModifiers())
    return key, modifiers
}

main :: () -> s32 {
    CanvasEventsReset()
    app_session = SessionOpen()
    if !SessionValid(app_session) { return 40 }
    if KeyF3 != 292 { return 32 }
    unused CanvasKeyDown("F3", "F3", false, 0)
    key, modifiers := Take()
    unused KeyboardSend(app_session, key)
    if key != KeyF3 || KeyboardTake(app_session) != KeyF3 || KeyboardTake(app_session) != 0 { return 33 }
    unused CanvasKeyUp("F3", "F3")
    CanvasEventsReset()
    unused CanvasKeyDown("Home", "Home", false, 0)
    unused CanvasKeyUp("Home", "Home")
    unused CanvasKeyDown("Digit4", "4", false, 0)
    unused CanvasKeyUp("Digit4", "4")
    // The app loop takes its input before the frame begins drawing.
    key, modifiers = Take()
    CanvasEventsBeginFrame()
    if key != 268 { return 1 }
    if !IsKeyPressed(52) { return 2 }
    CanvasEventsEndFrame()
    if canvas_keys_pressed_count != 0 || canvas_keys_released_count != 0 { return 3 }
    key, modifiers = Take()
    CanvasEventsBeginFrame()
    if key != 52 { return 4 }
    // Delivering an unread event must not replay its pressed/released flags.
    if canvas_keys_pressed_count != 0 || canvas_keys_released_count != 0 { return 5 }
    CanvasEventsEndFrame()
    key, modifiers = Take()
    if key != 0 { return 6 }
    // Events arriving while drawing yields survive cleanup, with no duplication.
    unused CanvasKeyDown("KeyA", "a", false, 0)
    CanvasEventsBeginFrame()
    key, modifiers = Take()
    if key != 65 { return 7 }
    unused CanvasKeyDown("KeyB", "b", false, 0)
    CanvasEventsEndFrame()
    if !CanvasQueueHas(*canvas_keys_pressed[0], canvas_keys_pressed_count, 66) { return 8 }
    key, modifiers = Take()
    if key != 66 { return 9 }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    key, modifiers = Take()
    if key != 0 || canvas_keys_pressed_count != 0 { return 10 }
    // Raylib's GetKeyPressed reads this frame's presses beside session delivery.
    unused CanvasKeyDown("KeyC", "c", false, 0)
    if GetKeyPressed() != 67 || GetKeyPressed() != 0 { return 16 }
    key, modifiers = Take()
    if key != 67 { return 17 }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    if GetKeyPressed() != 0 { return 18 }
    CanvasEventsReset()
    // Repeats keep their place after the fresh press, edit by edit.
    unused CanvasKeyDown("ArrowLeft", "ArrowLeft", false, 0)
    unused CanvasKeyDown("ArrowRight", "ArrowRight", false, 0)
    unused CanvasKeyDown("ArrowLeft", "ArrowLeft", true, 1)
    expected: [3]s32 = .[263, 262, 263]
    expected_modifiers: [3]s32 = .[0, 0, 1]
    for index: 0..2 {
        key, modifiers = Take()
        if key != expected[index] || modifiers != expected_modifiers[index] { return 11 }
    }
    CanvasEventsBeginFrame()
    CanvasEventsEndFrame()
    if canvas_keys_repeated_count != 0 { return 12 }
    key, modifiers = Take()
    if key != 0 { return 13 }
    unused CanvasKeyDown("KeyC", "c", false, 0)
    CanvasEventsReset()
    key, modifiers = Take()
    if key != 0 { return 14 }
    // A focus change cancels pending shortcuts and keeps typed text.
    unused CanvasKeyDown("KeyD", "d", false, 2)
    CanvasTyped(100)
    unused CanvasEvent(CanvasEventBlur, 0, 0, 0.0, 0.0, null, null)
    key, modifiers = Take()
    if key != 0 || TypedCodepointTake(app_session) != 100 { return 15 }
    // Complete shortcuts retain event flags after every physical key is up.
    CanvasEventsReset()
    unused CanvasKeyDown("ControlLeft", "Control", false, 2)
    unused CanvasKeyDown("KeyA", "a", false, 2)
    unused CanvasKeyUp("KeyA", "a")
    unused CanvasKeyUp("ControlLeft", "Control")
    unused CanvasKeyDown("KeyB", "b", false, 0)
    unused CanvasKeyUp("KeyB", "b")
    if CanvasHeldModifiers() != 0 || IsKeyDown(341) { return 19 }
    key, modifiers = Take()
    if key != 65 || modifiers != 2 || !TextFieldInputFor(key, modifiers, "").select_all { return 20 }
    key, modifiers = Take()
    if key != 66 || modifiers != 0 { return 21 }
    key, modifiers = Take()
    if key != 0 || modifiers != 0 { return 22 }
    // Repeats carry their own flags, even when the modifier changes while held.
    CanvasEventsReset()
    unused CanvasKeyDown("KeyA", "a", false, 2)
    unused CanvasKeyDown("KeyA", "A", true, 3)
    CanvasEventsBeginFrame()
    unused CanvasKeyDown("KeyA", "a", true, 0)
    unused CanvasKeyDown("KeyB", "b", false, 4)
    unused CanvasKeyUp("KeyA", "a")
    unused CanvasKeyUp("KeyB", "b")
    CanvasEventsEndFrame()
    keys: [5]s32 = .[65, 65, 65, 66, 0]
    flags: [5]s32 = .[2, 3, 0, 4, 0]
    for index: cast(s32)0..4 {
        key, modifiers = Take()
        if key != keys[index] || modifiers != flags[index] { return 23 + index }
    }
    // Typed text and the Enter after it commit together; Space types a space.
    CanvasEventsReset()
    CanvasTyped(104)
    unused CanvasKeyDown("Space", " ", false, 0)
    unused CanvasKeyDown("Enter", "Enter", false, 0)
    key, modifiers = Take()
    bytes: [8]u8
    if key != KeyEnter || TypedTextTake(app_session, bytes[:]) != "h " { return 34 }
    // Raylib's GetCharPressed takes this frame's characters; later ones survive.
    if GetCharPressed() != 104 || GetCharPressed() != 32 || GetCharPressed() != 0 { return 35 }
    CanvasTyped(120)
    CanvasEventsBeginFrame()
    CanvasTyped(121)
    CanvasEventsEndFrame()
    if GetCharPressed() != 121 || GetCharPressed() != 0 { return 36 }
    while TypedCodepointTake(app_session) != 0 {}
    // A bounded queue drops whole events, never just a key or its modifiers.
    CanvasEventsReset()
    for at: cast(s32)0..INPUT_DELIVERY_CAPACITY {
        unused CanvasKeyDown("ArrowUp", "ArrowUp", false, at % 8)
        unused CanvasKeyUp("ArrowUp", "ArrowUp")
    }
    if canvas_input.count != INPUT_DELIVERY_CAPACITY { return 28 }
    for at: cast(s32)0..INPUT_DELIVERY_CAPACITY - 1 {
        key, modifiers = Take()
        if key != 265 || modifiers != at % 8 { return 29 }
    }
    key, modifiers = Take()
    if key != 0 { return 30 }
    unused CanvasKeyDown("KeyA", "a", false, 7)
    unused CanvasEvent(CanvasEventBlur, 0, 0, 0.0, 0.0, null, null)
    key, modifiers = Take()
    if key != 0 || modifiers != 0 { return 31 }
    TreeInputReset(app_session)
    unused SessionClose(app_session)
    return 0
}
'''


def run(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)


def main():
    run(['git', 'check-ignore', 'build/scratch/canvas-app-key-queue/test'])
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        fixture = WORK / 'app_keys.zi'
        fixture.write_text(SOURCE)
        # No browser cursor/gamepad effects are needed by this native test.
        (WORK / 'page_input.zi').write_text('PageInputCursor :: (mode: s32) {}\nPageGamepadQuery :: (pad: s32, field: s32, index: s32) -> s32 { return 0 }\nPageGamepadAxis :: (pad: s32, axis: s32) -> float64 { return 0.0 }\n')
        paths = ['--root', WORK, '--module-path', ROOT / 'tests/page_fake',
                 '--module-path', ROOT / 'src/backend', *UI_MODULE_ARGS,
                 '--module-path', ZIRAN_ROOT / 'std']
        target = ['--target-os', 'web', '--target-cpu', 'wasm32']
        define = ['--define', 'PLATFORM_WEB']
        ir = WORK / 'ir'
        run([ZIRAN, 'ir', *paths, *target, *define, '-o', ir, fixture])
        for form in ('source', 'saved'):
            source = fixture if form == 'source' else ir / 'app_keys.zir'
            modules = paths if form == 'source' else ['--root', ir]
            run([ZIRAN, 'build', '--target=c', '--exe', '--entry', 'app_keys:main',
                 *modules, *target, *define, '-o', WORK / 'native', source])
            run([WORK / 'native/app_keys'])
            print(f'{form}: native Canvas ordered key delivery passed', flush=True)


if __name__ == '__main__':
    main()
