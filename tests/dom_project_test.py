#!/usr/bin/env python3
"""Build a real Ziran application with the semantic DOM profile and inspect it."""

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


with tempfile.TemporaryDirectory(prefix="dom-project-", dir=ROOT / "build") as directory:
    project = Path(directory)
    (project / "src").mkdir()
    (project / "ziran.toml").write_text(
        '[package]\nname = "dom_probe"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
        'ref = "master"\n\n'
        '[dependencies.Kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
        'ref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "dom"\n'
    )
    (project / "ziran.local.toml").write_text(
        f'[overrides]\nKryon = "{ROOT}"\n'
        f'ziran = "{ROOT.parent / "ziran"}"\n'
    )
    (project / "src/app.zi").write_text(
        'using UI :: #import "Widgets";\n'
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
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "Kryon", "build", "--profile", "web"], project)
    output = project / "build/dom_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    assert next((project / "build/generated/web/ir").glob("dom_run.zir"), None)
    assert next((project / "build/generated/web/c").glob("dom_run.c"), None)
    run(["node", ROOT / "tests/dom_browser.mjs", output.as_uri(), project / "browser-profile"], project)
print("Semantic DOM Ziran project: manifest, saved IR, generated C, Emscripten page, and browser semantics passed")
