#!/usr/bin/env python3
"""Build a Canvas2D application that reads and writes the browser route."""

import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
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
        '[package]\nname = "route_probe"\nentry = "src/app.zi"\n'
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
        "ziran": str(ZIRAN_ROOT),
    }
    if os.environ.get("RAYLIB_SOURCE"):
        overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
    (project / "ziran.local.toml").write_text(
        "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
    )
    (project / "src/app.zi").write_text(
        'using UI :: #import "kryon/Widgets";\n'
        '#import "c_string"\n'
        '#import "kryon/PageRoute"\n'
        'host_api :: #system_library "host_api";\n'
        'RequestWindowClose :: () #foreign host_api;\n'
        'Same :: (left: *u8, right: string) -> bool {\n'
        '    index: s32 = 0\n'
        '    while index < cast(s32)right.count {\n'
        '        if left[index] != right[index] { return false }\n'
        '        index += 1\n'
        '    }\n'
        '    return left[index] == cast(u8)0\n}\n'
        '#program_export\n'
        'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
        '    path: *u8 = GetRoutePath()\n'
        '    if path[0] != cast(u8)47 { return 0 }\n'
        '    before: s32 = GetRouteVersion()\n'
        '    target: [512]u8\n'
        '    length: s32 = 0\n'
        '    while path[length] != cast(u8)0 && length < 500 {\n'
        '        target[length] = path[length]\n'
        '        length += 1\n'
        '    }\n'
        '    suffix: string = "#round"\n'
        '    index: s32 = 0\n'
        '    while index < cast(s32)suffix.count {\n'
        '        target[length + index] = suffix[index]\n'
        '        index += 1\n'
        '    }\n'
        '    target[length + index] = cast(u8)0\n'
        '    if !Same(GetRouteQuery(), "?probe=1") { return 0 }\n'
        '    PushRoute(*target[0])\n'
        '    if !Same(GetRouteHash(), "#round") { return 0 }\n'
        '    if !Same(GetRouteQuery(), "?probe=1") { return 0 }\n'
        '    if GetRouteVersion() <= before { return 0 }\n'
        '    ReplaceRoute(*target[0])\n'
        '    if !Same(GetRouteHash(), "#round") { return 0 }\n'
        '    if !Same(GetRouteQuery(), "?probe=1") { return 0 }\n'
        '    RequestWindowClose()\n'
        '    return 0\n}\n'
    )
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/route_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    run(["node", ROOT / "tests/canvas_backend_browser.mjs",
         output.as_uri() + "?probe=1", project / "browser-profile"], project)
print("Browser route: path, query, hash, push, replace and version passed")
