#!/usr/bin/env python3
"""Build an application from TSX with the Canvas2D profile and load it."""

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
        '[package]\nname = "canvas_probe"\nentry = "src/app.tsx"\n'
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
    (project / "src/app.tsx").write_text('''import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Text } from "kryon/tsx/text";
import { WindowQuit } from "kryon/window";

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Canvas probe", bounds: frame.viewport}}/>;
    WindowQuit();
}

export function main(): s32 { return App.Run({title: "canvas probe"}, Frame); }
''')
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/canvas_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    assert "<title>canvas_probe</title>" in output.read_text(errors="replace"), "page is not named after the program"
    assert (project / "build/generated/web/ir/app.zir").is_file(), "the page has no generated program entry"
    assert (project / "build/generated/web/c/app.c").is_file(), "the page has no generated program C"
    run([JS_RUNTIME, ROOT / "tests/canvas_backend_browser.mjs",
         output.as_uri(), project / "browser-profile"], project)
print("Canvas TSX project: manifest, saved IR, generated C, Emscripten page, and browser exit passed")
