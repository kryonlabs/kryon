#!/usr/bin/env python3
"""Build a real Ziran application with the Canvas2D profile and load it."""

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = ROOT.parent / "ziran/build/bin/ziran"
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY"):
    ENV.pop(name, None)
ENV.setdefault("EM_CACHE", str(ROOT / "build/emscripten-cache"))


def run(command, directory):
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV, check=True)


with tempfile.TemporaryDirectory(prefix="canvas-project-", dir=ROOT / "build") as directory:
    project = Path(directory)
    (project / "src").mkdir()
    (project / "ziran.toml").write_text(
        '[package]\nname = "canvas_probe"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
        'ref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
        'ref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "canvas"\n'
    )
    overrides = {
        "kryon": str(ROOT),
        "ziran": str(ROOT.parent / "ziran"),
    }
    if os.environ.get("RAYLIB_SOURCE"):
        overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
    (project / "ziran.local.toml").write_text(
        "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
    )
    (project / "src/app.zi").write_text(
        'using UI :: #import "kryon/Widgets";\n'
        'host_api :: #system_library "host_api";\n'
        'RequestWindowClose :: () #foreign host_api;\n'
        '#program_export\n'
        'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
        '    RequestWindowClose()\n'
        '    return 0\n}\n'
    )
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/canvas_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    assert next((project / "build/generated/web/ir").glob("*_canvas_run.zir"), None)
    assert next((project / "build/generated/web/c").glob("*_canvas_run.c"), None)
    run(["node", ROOT / "tests/canvas_backend_browser.mjs",
         output.as_uri(), project / "browser-profile"], project)
print("Canvas Ziran project: manifest, saved IR, generated C, Emscripten page, and browser exit passed")
