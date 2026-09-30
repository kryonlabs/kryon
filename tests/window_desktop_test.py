"""Hide, show, close interception, and quit on the Ziran SDL/Cairo host.

Everything runs on a private Xvfb display. The driver only addresses the
window owned by the probe process it started.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from raylib_project_test import run


ROOT = Path(__file__).resolve().parents[1]
# The sibling checkout CI uses, else the org-grouped local layout.
ZIRAN_ROOT = Path(os.environ.get("ZIRAN_ROOT") or next(
    (path for path in (ROOT.parent / "ziran", ROOT.parent.parent / "ziranlang/ziran")
     if path.is_dir()), ROOT.parent / "ziran"))
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")

APP = '''using UI :: #import "Widgets";
#import "Window"

frames: s32;
hidden_at: s32;
intercepting: bool;

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    frames += 1
    // Hosts that cannot intercept closing (the terminal) only draw.
    if frames == 1 { intercepting = WindowInterceptClose(true) }
    if frames > 3000 { return 1 }
    if intercepting && hidden_at == 0 && WindowTakeCloseRequest() {
        WindowHide()
        hidden_at = frames
    }
    if hidden_at > 0 && frames == hidden_at + 1 && WindowVisible() {
        return 1
    }
    if hidden_at > 0 && frames == hidden_at + 20 { WindowShow() }
    if hidden_at > 0 && frames == hidden_at + 22 {
        if !WindowVisible() { return 1 }
        WindowQuit()
    }
    text: TextProps
    text.key = cast(u64)1
    text.bounds = Rectangle.{20.0, 20.0, 300.0, 30.0}
    text.text = "window probe"
    Text(session, text)
    return 0
}
'''

DRIVER = '''import subprocess
import sys
import time
from Xlib import X, display, protocol

app = subprocess.Popen(["./build/window_probe-desktop"])
screen = display.Display()
window_id = None
for _ in range(100):
    found = subprocess.run(["xdotool", "search", "--pid", str(app.pid)],
                           capture_output=True, text=True)
    ids = found.stdout.split()
    if ids:
        window_id = int(ids[0])
        break
    time.sleep(0.1)
if window_id is None:
    app.kill()
    sys.exit("probe window did not appear")
time.sleep(0.5)
window = screen.create_resource_object("window", window_id)
protocols = screen.intern_atom("WM_PROTOCOLS")
delete = screen.intern_atom("WM_DELETE_WINDOW")
event = protocol.event.ClientMessage(window=window, client_type=protocols,
                                     data=(32, [delete, X.CurrentTime, 0, 0, 0]))
window.send_event(event, event_mask=X.NoEventMask)
screen.flush()
time.sleep(0.8)
if app.poll() is not None:
    sys.exit("closing the window quit instead of reporting a request")
if window.get_attributes().map_state == X.IsViewable:
    app.kill()
    sys.exit("window stayed visible after WindowHide")
try:
    code = app.wait(timeout=20)
except subprocess.TimeoutExpired:
    app.kill()
    sys.exit("probe did not quit through WindowQuit")
if code != 0:
    sys.exit(code)

# Interception covers the close button only: the session ending (SIGTERM,
# logout) still quits an app that would otherwise hide to its tray.
app = subprocess.Popen(["./build/window_probe-desktop"])
for _ in range(100):
    if subprocess.run(["xdotool", "search", "--pid", str(app.pid)],
                      capture_output=True).stdout.split():
        break
    time.sleep(0.1)
time.sleep(0.5)
app.terminate()
try:
    code = app.wait(timeout=5)
except subprocess.TimeoutExpired:
    app.kill()
    sys.exit("SIGTERM was treated as a close request")
sys.exit(code)
'''


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        env.pop(name, None)
    return env


def main():
    for tool in ("xvfb-run", "xdotool"):
        assert shutil.which(tool), f"Install {tool} to test the desktop window"
    with tempfile.TemporaryDirectory(prefix="kryon-window-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "window_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.Kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.Kryon]\ndefault_profile = "desktop"\n\n'
            '[tool.Kryon.profiles.desktop]\nbackend = "desktop"\n\n'
            '[tool.Kryon.profiles.tui]\nbackend = "terminal"\n'
        )
        (project / "ziran.local.toml").write_text(
            f'[overrides]\nKryon = "{ROOT}"\n'
            f'ziran = "{ZIRAN_ROOT}"\n'
        )
        (project / "src/app.zi").write_text(APP)
        (project / "driver.py").write_text(DRIVER)
        env = private_environment()
        run([ZIRAN, "lock"], project, env)
        run([ZIRAN, "tool", "Kryon", "build", "--profile", "desktop"],
            project, env)
        run(["xvfb-run", "-a", sys.executable, "driver.py"], project, env)
        # The terminal host links the same window calls and ignores them.
        run([ZIRAN, "tool", "Kryon", "build", "--profile", "tui"],
            project, env)
        result = subprocess.run(["./build/window_probe-tui"], cwd=project,
                                env=env, capture_output=True, text=True,
                                timeout=30)
        assert result.returncode == 0, result.stderr
        assert "window probe" in result.stdout, result.stdout
    print("Desktop window hide, show, close interception, and quit: passed")


if __name__ == "__main__":
    main()
