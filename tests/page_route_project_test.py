#!/usr/bin/env python3
"""Build a Canvas2D application that reads and writes the browser route."""

from toolchain import JS_RUNTIME, pinned, kryon_only

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
    (project / "ziran.toml").write_text(pinned(
        '[package]\nname = "route_probe"\nentry = "src/app.tsx"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
        'ref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
        'ref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "canvas"\n'
    ))
    overrides = {
        "kryon": str(ROOT),
        "ziran": str(ZIRAN_ROOT),
    }
    (project / "ziran.local.toml").write_text(kryon_only(
        "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
    ))
    (project / "src/route_probe.zi").write_text(
        '#import "kryon/PageRoute"\n'
        'Same :: (left: *u8, right: string) -> bool {\n'
        '    index: s32 = 0\n'
        '    while index < cast(s32)right.count {\n'
        '        if left[index] != right[index] { return false }\n'
        '        index += 1\n'
        '    }\n'
        '    return left[index] == cast(u8)0\n}\n'
        'CheckBrowserRoute :: () -> bool {\n'
        '    path: *u8 = GetRoutePath()\n'
        '    if path[0] != cast(u8)47 { return false }\n'
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
        '    if !Same(GetRouteQuery(), "?probe=1") { return false }\n'
        '    origin: *u8 = GetRouteOrigin()\n'
        '    if origin[0] != cast(u8)102 { return false }\n'  # f
        '    if origin[1] != cast(u8)105 { return false }\n'  # i
        '    if origin[2] != cast(u8)108 { return false }\n'  # l
        '    if origin[3] != cast(u8)101 { return false }\n'  # e
        '    if origin[4] != cast(u8)58 { return false }\n'   # :
        '    PushRoute(*target[0])\n'
        '    if !Same(GetRouteHash(), "#round") { return false }\n'
        '    if !Same(GetRouteQuery(), "?probe=1") { return false }\n'
        '    if GetRouteVersion() <= before { return false }\n'
        '    ReplaceRoute(*target[0])\n'
        '    return Same(GetRouteHash(), "#round") && Same(GetRouteQuery(), "?probe=1")\n}\n'
    )
    (project / "src/app.tsx").write_text('''import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { WindowQuit } from "kryon/window";
import { CheckBrowserRoute } from "route_probe";

export function Frame(session: Session, viewport: Rectangle): s32 {
    if (CheckBrowserRoute()) { WindowQuit(); }
    return s32(0);
}
''')
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/route_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    run([JS_RUNTIME, ROOT / "tests/canvas_backend_browser.mjs",
         output.as_uri() + "?probe=1", project / "browser-profile"], project)
print("Browser route: path, query, hash, push, replace and version passed")
