#!/usr/bin/env python3
"""Build a real Ziran application with the semantic DOM profile and inspect it."""

import os
import fcntl
from pathlib import Path
import subprocess
import tempfile

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
    ENV.pop(name, None)
ENV["YUE_DESKTOP_RECOVERY"] = "0"
ENV.setdefault("EM_CACHE", str(ROOT / "build/emscripten-cache"))


def run(command, directory):
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV, check=True, timeout=300)


project = ROOT / "build/dom-project"
project.mkdir(parents=True, exist_ok=True)
with (project / "lock").open("w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    (project / "src").mkdir(exist_ok=True)
    (project / "ziran.toml").write_text(
        '[package]\nname = "dom_probe"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
        'ref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
        'ref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "dom"\n'
    )
    if os.environ.get("KRYON_DOM_INTERACTION"):
        with (project / "ziran.toml").open("a") as manifest:
            manifest.write('\n[dependencies.Kss]\ngit = "https://github.com/kryonlabs/kss.git"\nref = "master"\n')
    overrides = {
        "kryon": str(ROOT),
        "ziran": str(ZIRAN_ROOT),
    }
    if os.environ.get("KRYON_DOM_INTERACTION"):
        local_kss = Path(os.environ.get("KSS_ROOT", ROOT.parent / "packages/kss"))
        if local_kss.is_dir():
            overrides["Kss"] = str(local_kss)
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
        '    page: PageProps\n'
        '    page.bounds = viewport\n'
        '    page.title = "DOM Probe"\n'
        '    opened: PageResult = Page(session, page)\n'
        '    heading: TextProps\n'
        '    heading.bounds = Rectangle.{10.0, 10.0, 240.0, 32.0}\n'
        '    heading.text = "Semantic DOM"\n'
        '    heading.semantic_kind = SemanticHeading()\n'
        '    heading.heading_level = 1\n'
        '    Text(session, heading)\n'
        '    button: ButtonProps\n'
        '    button.bounds = Rectangle.{10.0, 50.0, 100.0, 32.0}\n'
        '    button.label = "Choose"\n'
        '    button.disabled = true\n'
        '    Button(session, button)\n'
        '    link: LinkProps\n'
        '    link.bounds = Rectangle.{10.0, 90.0, 100.0, 24.0}\n'
        '    link.text = "Docs"\n'
        '    link.link = "https://example.test/docs"\n'
        '    Link(session, link)\n'
        '    RequestWindowClose()\n'
        '    if opened.opened { End(session) }\n'
        '    return 0\n'
        '}\n'
    )
    if os.environ.get("KRYON_DOM_INTERACTION"):
        (project / "src/app.zi").write_text((ROOT / "tests/fixtures/dom_interaction.zi").read_text())
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/dom_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    assert next((project / "build/generated/web/ir").glob("*_dom_run.zir"), None)
    assert next((project / "build/generated/web/c").glob("*_dom_run.c"), None)
    run(["node", ROOT / "tests/dom_browser.mjs", output.as_uri(), project / "browser-profile"], project)
print("Semantic DOM Ziran project: manifest, saved IR, generated C, Emscripten page, and browser semantics passed")
