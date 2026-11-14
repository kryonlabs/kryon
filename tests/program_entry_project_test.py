#!/usr/bin/env python3
"""Build a program entry (main plus App.Run) through the project tool.

The entry is the maintained examples/window.zi. Its canvas profile must give a
page whose generated host runs that main, and its desktop profile an
executable that maps the program's window. Both run on private displays.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from toolchain import JS_RUNTIME, ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY",
             "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER"):
    ENV.pop(name, None)
ENV.setdefault("EM_CACHE", str(ROOT / "build/emscripten-cache"))

DRIVER = '''import subprocess
import sys
import time

app = subprocess.Popen(["./build/window_program-desktop"])
name = ""
for _ in range(100):
    found = subprocess.run(["xdotool", "search", "--pid", str(app.pid)],
                           capture_output=True, text=True).stdout.split()
    if found:
        name = subprocess.run(["xdotool", "getwindowname", found[0]],
                              capture_output=True, text=True).stdout.strip()
        break
    if app.poll() is not None:
        sys.exit(f"program exited with {app.returncode} before opening its window")
    time.sleep(0.1)
app.terminate()
try:
    app.wait(timeout=5)
except subprocess.TimeoutExpired:
    app.kill()
    sys.exit("program did not end on SIGTERM")
if name != "Window":
    sys.exit(f"expected the program's window title, got {name!r}")
'''


def run(command, directory):
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV, check=True)


for tool in ("xvfb-run", "xdotool"):
    assert shutil.which(tool), f"Install {tool} to test a program entry's window"

with tempfile.TemporaryDirectory(prefix="program-entry-", dir=ROOT / "build") as directory:
    project = Path(directory)
    (project / "src").mkdir()
    (project / "ziran.toml").write_text(
        '[package]\nname = "window_program"\nentry = "src/window.zi"\n'
        'module_roots = ["src"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
        'ref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
        'ref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "desktop"\n\n'
        '[tool.kryon.profiles.desktop]\nbackend = "desktop"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "canvas"\n\n'
        '[tool.kryon.profiles.portable]\nbackend = "desktop"\ncodegen = "zx"\n'
    )
    (project / "ziran.local.toml").write_text(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n'
    )
    shutil.copyfile(ROOT / "examples/window.zi", project / "src/window.zi")
    (project / "driver.py").write_text(DRIVER)
    run([ZIRAN, "lock"], project)

    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    page = project / "build/window_program-web.html"
    assert page.is_file() and page.stat().st_size > 100_000
    host = project / "build/generated/web/c/kryon_program_host.c"
    assert host.is_file(), "the page has no generated program host"
    assert (project / "build/generated/web/ir/kryon_program_host.zir").is_file()
    assert not list((project / "build/generated/web/c").glob("*_canvas_run.c")), \
        "a program entry must not also link the frame host"
    run([JS_RUNTIME, ROOT / "tests/program_entry_browser.mjs",
         page.as_uri(), project / "browser-profile"], project)

    run([ZIRAN, "tool", "kryon", "build", "--profile", "desktop"], project)
    run(["xvfb-run", "-a", sys.executable, "driver.py"], project)
    # An empty window edits no text and is not under test, so it carries
    # neither the Unicode tables nor the test driver: those come with the
    # editing widgets and with a build made for a test.
    def generated():
        return sorted(path.name for path in (project / "build/generated/desktop/c").glob("*.c"))
    plain = generated()
    assert plain, "the desktop profile kept no generated C to inspect"
    for part in ("grapheme", "test_driver_engine"):
        assert not [name for name in plain if part in name], \
            f"an empty window carries {part}: {plain}"

    # A build made for a test asks for the driver and gets it; the same project
    # built again without the variable drops it.
    tested = dict(ENV, KRYON_TEST_DRIVER="1")
    subprocess.run([str(ZIRAN), "tool", "kryon", "build", "--profile", "desktop"],
                   cwd=project, env=tested, check=True)
    assert [name for name in generated() if "test_driver_engine" in name], \
        "KRYON_TEST_DRIVER did not include the test driver"
    run([ZIRAN, "tool", "kryon", "build", "--profile", "desktop"], project)
    assert generated() == plain, "the test driver stayed after an ordinary rebuild"

    # A portable bundle has no window host, so the tool names the mismatch.
    portable = subprocess.run(
        [str(ZIRAN), "tool", "kryon", "build", "--profile", "portable"],
        cwd=project, env=ENV, capture_output=True, text=True)
    assert portable.returncode != 0
    assert "needs codegen c99" in portable.stdout + portable.stderr, \
        portable.stdout + portable.stderr
print("Program entry: canvas page runs main, desktop executable maps its window, "
      "portable profile is refused")
