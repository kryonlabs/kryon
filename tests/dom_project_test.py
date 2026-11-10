#!/usr/bin/env python3
"""Build a real Ziran application with the semantic DOM profile and inspect it."""

import os
import fcntl
from pathlib import Path
import subprocess
import tempfile
import struct
import sys
import zlib

from toolchain import JS_RUNTIME, ZIRAN, ZIRAN_ROOT, chromium_temp_directory

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
        '[tool.kryon.profiles.web]\nbackend = "dom"\nassets = "assets"\n'
    )
    if os.environ.get("KRYON_DOM_INTERACTION"):
        with (project / "ziran.toml").open("a") as manifest:
            manifest.write('\n[dependencies.css]\ngit = "https://github.com/kryonlabs/css.git"\nref = "master"\n')
    # A two-color encoded image exercises asset loading and mirrored UVs.
    assets = project / "assets"
    assets.mkdir(exist_ok=True)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    (assets / "mesh.png").write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 1, 8, 6, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(bytes([0,255,0,0,255,0,0,255,255]))) + chunk(b"IEND", b""))
    # The profile assets directory is embedded by the package build.
    if os.environ.get("KRYON_DOM_IMAGE_DECODE"):
        # A page image, bytes no browser decodes, and one whose img decode
        # the driver rejects.
        rows = b"".join(b"\0" + bytes((x * 7 + y * 3) % 256 for x in range(300) for _ in range(3)) for y in range(200))
        page = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 300, 200, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
        (assets / "page.png").write_bytes(page)
        (assets / "rejected.png").write_bytes(page)
        (assets / "broken.png").write_bytes(b"not an image")
        names = ["page.png", "rejected.png", "broken.png"]
        # Other textures loaded while the page image is hidden.
        for index in range(6):
            rows = b"".join(b"\0" + bytes((index * 40, 255 - index * 40, 128)) * 40 for _ in range(30))
            names.append(f"other{index}.png")
            (assets / names[-1]).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 40, 30, 8, 2, 0, 0, 0)) +
                                              chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
    overrides = {
        "kryon": str(ROOT),
        "ziran": str(ZIRAN_ROOT),
    }
    if os.environ.get("KRYON_DOM_INTERACTION"):
        local_css = Path(os.environ.get("CSS_ROOT", ROOT.parent / "packages/css"))
        if local_css.is_dir():
            overrides["css"] = str(local_css)
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
        '    points: [4]Vector2 = .{.{180.0,160.0},.{100.0,160.0},.{180.0,200.0},.{100.0,200.0}}\n'
        '    Image(session, ImageProps.{.key = 101, .bounds = .{100.0,160.0,80.0,40.0}, .asset_path = "assets/mesh.png", .mesh = .{2,2,points[:]}})\n'
        '    // A separate fallback tile used to receive the deferred mesh.\n'
        '    Image(session, ImageProps.{.key = 102, .bounds = .{12.0,9.0,38.0,38.0}, .rotation = 1.0, .asset_path = "assets/mesh.png"})\n'
        '    points[0].y += 80.0; points[1].y += 80.0; points[2].y += 80.0; points[3].y += 80.0\n'
        '    // Last mesh must finish before the frame restores the page context.\n'
        '    Image(session, ImageProps.{.key = 103, .bounds = .{100.0,240.0,80.0,40.0}, .asset_path = "assets/mesh.png", .mesh = .{2,2,points[:]}})\n'
        '    RequestWindowClose()\n'
        '    if opened.opened { End(session) }\n'
        '    return 0\n'
        '}\n'
    )
    if os.environ.get("KRYON_DOM_INTERACTION"):
        (project / "src/app.zi").write_text((ROOT / "tests/fixtures/dom_interaction.zi").read_text())
    driver = ROOT / "tests/dom_browser.mjs"
    if os.environ.get("KRYON_DOM_PRESS"):
        (project / "src/app.zi").write_text((ROOT / "tests/fixtures/dom_press_point.zi").read_text())
        driver = ROOT / "tests/dom_press_point_browser.mjs"
    runner = JS_RUNTIME
    if os.environ.get("KRYON_DOM_IMAGE_DECODE"):
        (project / "src/app.zi").write_text((ROOT / "tests/fixtures/dom_image_decode.zi").read_text())
        driver = ROOT / "tests/dom_image_decode_browser.py"
        runner = sys.executable
    if os.environ.get("KRYON_DOM_PRESS_MOVE"):
        (project / "src/app.zi").write_text((ROOT / "tests/fixtures/dom_press_move.zi").read_text())
        driver = ROOT / "tests/dom_press_move_browser.py"
        runner = sys.executable
    run([ZIRAN, "lock"], project)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], project)
    output = project / "build/dom_probe-web.html"
    assert output.is_file() and output.stat().st_size > 100_000
    assert next((project / "build/generated/web/ir").glob("*_dom_run.zir"), None)
    assert next((project / "build/generated/web/c").glob("*_dom_run.c"), None)
    with chromium_temp_directory() as sockets:
        ENV["TMPDIR"] = sockets
        run([runner, driver, output.as_uri(), project / "browser-profile"], project)
print("Semantic DOM Ziran project: manifest, saved IR, generated C, Emscripten page, and browser semantics passed")
