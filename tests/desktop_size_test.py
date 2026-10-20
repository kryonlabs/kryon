"""Open the Ziran SDL/Cairo desktop at a chosen size and resize it.

The probe runs on a private Xvfb display and only draws its own window.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from raylib_project_test import png_pixels, run


from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")

APP = '''Desktop :: #import "Desktop";
#import "geometry"
#import "byte_text_linux"
#import "c_string"
sdl :: #system_library "SDL2";
NativeTitle :: (window: *void) -> *u8 #foreign sdl "SDL_GetWindowTitle";
WindowId :: (window: *void) -> u32 #foreign sdl "SDL_GetWindowID";
Duplicate :: (text: *u8) -> *u8 #foreign sdl "SDL_strdup";
Push :: (event: *void) -> s32 #foreign sdl "SDL_PushEvent";
Drop :: struct { kind: u32; time: u32; path: *u8; window: u32; padding: [36]u8; }
SendDrop :: (window: u32, path: *u8, kind: u32 = 0x1000) -> bool {
    event: Drop
    event.kind = kind
    event.path = Duplicate(path)
    event.window = window
    return Push(cast(*void)*event) == 1
}

#program_export
main :: () -> s32 {
    if !Desktop.OpenDesktopSized(320, 200) { return 2 }
    if !Desktop.DesktopSetTitle("Time Sheet.gnumeric") { return 10 }
    if TextFromCString(NativeTitle(Desktop.DesktopWindow())) != "Time Sheet.gnumeric" { return 11 }
    own := WindowId(Desktop.DesktopWindow())
    first: [64]u8
    second: [64]u8
    if !CopyCString("/tmp/Time Sheet.gnumeric", first[:]) ||
       !CopyCString("/tmp/two", second[:]) { return 12 }
    if !SendDrop(own + 1, *second[0]) || !SendDrop(own, *second[0], 0x1001) ||
       !SendDrop(own, *first[0]) || !SendDrop(own, *second[0]) { return 12 }
    if !Desktop.PollDesktop() { return 13 }
    small: [2]u8
    path: [4096]u8
    if Desktop.DesktopTakeDroppedFile(small[:]) { return 14 }
    if !Desktop.DesktopTakeDroppedFile(path[:]) || TextFromCString(*path[0]) != "/tmp/Time Sheet.gnumeric" { return 15 }
    if !Desktop.DesktopTakeDroppedFile(path[:]) || TextFromCString(*path[0]) != "/tmp/two" { return 16 }
    if Desktop.DesktopTakeDroppedFile(path[:]) { return 17 }
    viewport: Rectangle = Desktop.DesktopViewport()
    if viewport.width != 320.0 || viewport.height != 200.0 { return 3 }
    if !Desktop.ResizeDesktop(200, 120) { return 4 }
    // The size changes when the next frame begins, not mid-frame.
    viewport = Desktop.DesktopViewport()
    if viewport.width != 320.0 { return 5 }
    Desktop.BeginDesktopFrame()
    viewport = Desktop.DesktopViewport()
    if viewport.width != 200.0 || viewport.height != 120.0 { return 6 }
    if !Desktop.EndDesktopFrame() { return 7 }
    if !Desktop.CaptureConfigured() { return 8 }
    if Desktop.ResizeDesktop(0, 120) { return 9 }
    Desktop.CloseDesktop()
    return 0
}
'''


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(name, None)
    return env


def main():
    assert shutil.which("xvfb-run"), "Install xvfb-run to test the desktop window"
    with tempfile.TemporaryDirectory(prefix="kryon-desktop-size-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "size_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n'
        )
        overrides = {
            "kryon": str(ROOT),
            "ziran": str(ZIRAN_ROOT),
        }
        if os.environ.get("RAYLIB_SOURCE"):
            overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
        (project / "ziran.local.toml").write_text(
            "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        (project / "src/app.zi").write_text(APP)
        env = private_environment()
        run([ZIRAN, "lock"], project, env)
        generated = project / "generated"
        run([ZIRAN, "build", "--project", "--target=c", "--entry", "app:main",
             "-o", str(generated), "src/app.zi"], project, env)
        libraries = subprocess.run(["pkg-config", "--libs", "sdl2", "cairo", "freetype2", "fontconfig", "pangocairo"],
                                   check=True, capture_output=True,
                                   text=True).stdout.split()
        run([os.environ.get("CC", "cc"), "-std=c99",
             f"-I{ZIRAN_ROOT / 'include'}", f"-I{generated}",
             *map(str, sorted(generated.glob("*.c"))), *libraries, "-lm",
             "-o", "probe"], project, env)

        capture = project / "capture.png"
        env["KRYON_CAPTURE_PATH"] = str(capture)
        run(["xvfb-run", "-a", "./probe"], project, env)
        width, height = png_pixels(capture)[:2]
        assert (width, height) == (200, 120), (width, height)
    print("desktop Ziran host: title, queued file drops, window scoping, sized window and deferred resize passed")


if __name__ == "__main__":
    main()
