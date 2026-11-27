#!/usr/bin/env python3
"""Build a program entry (main plus App.Run) through the project tool.

The entry is the maintained examples/window.tsx. Its canvas profile must give a
page whose generated host runs that main, and its desktop profile an
executable that maps the program's window. Both run on private displays.

The editor preview marks a page's canvas `data-transparent`: window.tsx's page
then leaves every pixel at alpha 0, and the maintained examples/hello.tsx still
draws its text over a transparent background. Unmarked, both are opaque.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from toolchain import JS_RUNTIME, ZIRAN, ZIRAN_ROOT, pinned, kryon_only

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
    # SDL names its window just after creating it, so a window found first
    # can still be unnamed; wait for the name.
    for window in found:
        name = subprocess.run(["xdotool", "getwindowname", window],
                              capture_output=True, text=True).stdout.strip()
        if name:
            break
    if name:
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


def browse(page, title, mode):
    """Load PAGE, or with mode other than opaque its transparent copy, the
    way the editor preview marks it, and check its canvas by MODE."""
    if mode != "opaque":
        html = page.read_text()
        assert html.count('<canvas id="canvas"') == 1, f"{page} has no single page canvas"
        page = page.with_name(page.stem + "-transparent.html")
        page.write_text(html.replace('<canvas id="canvas"', '<canvas id="canvas" data-transparent'))
    run([JS_RUNTIME, ROOT / "tests/program_entry_browser.mjs", page.as_uri(),
         page.with_suffix(".profile"), title, mode], page.parent)


def manifest(name, entry):
    return (
        f'[package]\nname = "{name}"\nentry = "src/{entry}"\n'
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


def project_from(directory, name, example):
    (directory / "src").mkdir(parents=True)
    (directory / "ziran.toml").write_text(pinned(manifest(name, example)))
    (directory / "ziran.local.toml").write_text(kryon_only(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n'
    ))
    shutil.copyfile(ROOT / "examples" / example, directory / "src" / example)
    run([ZIRAN, "lock"], directory)


for tool in ("xvfb-run", "xdotool"):
    assert shutil.which(tool), f"Install {tool} to test a program entry's window"

with tempfile.TemporaryDirectory(prefix="program-entry-", dir=ROOT / "build") as directory:
    project = Path(directory) / "window"
    project_from(project, "window_program", "window.tsx")
    (project / "driver.py").write_text(DRIVER)

    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    page = project / "build/window_program-web.html"
    assert page.is_file() and page.stat().st_size > 100_000
    host = project / "build/generated/web/c/window.c"
    assert host.is_file(), "the page has no generated program entry"
    assert (project / "build/generated/web/ir/window.zir").is_file()
    assert not (host.parent / "kryon_program_host.c").exists(), \
        "a program entry must not also link a generated main"
    assert not list((project / "build/generated/web/c").glob("*_canvas_run.c")), \
        "a program entry must not also link the frame host"
    browse(page, "Window", "opaque")
    browse(page, "Window", "clear")

    hello = Path(directory) / "hello"
    project_from(hello, "hello_program", "hello.tsx")
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], hello)
    hello_page = hello / "build/hello_program-web.html"
    browse(hello_page, "Hello", "opaque")
    browse(hello_page, "Hello", "drawn")

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

    # A zx profile's bundle is the program's own main; kryon/app gives it the
    # ZX window, so the tool builds it without a generated host.
    run([ZIRAN, "tool", "kryon", "build", "--profile", "portable"], project)
    bundle = project / "build/window_program-portable.zx"
    assert bundle.is_file() and bundle.stat().st_size > 0, "the zx profile built no bundle"
    assert not (project / "build/generated/portable/ir/kryon_program_host.zir").exists(), \
        "a zx program bundle must not add a generated main"
print("Program entry: canvas page runs main, opaque unless its canvas is transparent, "
      "desktop executable maps its window, zx profile bundles the program's main")
