#!/usr/bin/env python3
"""Build a raylib project for platform = "web" and check its page in private
headless Chromium: a custom host draws a 3D cube between BeginRaylibFrame and
the Kryon Frame, the Kryon UI paints on top, and the profile's assets and
preload directories are readable from the page's file system."""

import functools
import http.server
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading

from raylib_project_test import png_pixels, rgb, solid_png
from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]

MANIFEST = '''[package]
name = "raylib_web_probe"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"

[tool.kryon]
default_profile = "web"

[tool.kryon.profiles.web]
backend = "raylib"
platform = "web"
host = "src/host.zi"
assets = "assets"
preload = "data/levels@levels"

[tool.kryon.install]
name = "Raylib Web Probe"
'''

APP = '''using UI :: #import "Widgets";
#import "std/file"

// The UI paints only once the preloaded level file reads back as written.
#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    file := OpenRead("levels/probe.txt")
    if !file.valid { return 0 }
    bytes: [16]u8
    count := Read(file, bytes[:])
    unused CloseFile(file)
    if count != 5 || bytes[0] != cast(u8)107 || bytes[4] != cast(u8)110 { return 0 }
    panel: BoxProps
    panel.key = cast(u64)1
    panel.bounds = Rectangle.{20.0, 20.0, 160.0, 80.0}
    panel.fill = Color.{30, 60, 200, 255}
    unused Box(session, panel)
    image: ImageProps
    image.key = cast(u64)2
    image.bounds = Rectangle.{440.0, 20.0, 64.0, 64.0}
    image.asset_path = "assets/red.png"
    Image(session, image)
    return 0
}
'''

HOST = '''#import "kryon/raylib_runtime"
#import "kryon/session"
#import "kryon/tree"
#import "kryon/tree_draw"
#import "kryon/tree_input"
#import "kryon/frame_snapshot"
#import "kryon/geometry"
#import "kryon/drawing_props"
App :: #import "app";

using FrameStatus;

probe_raylib :: #system_library "raylib";
Vector3 :: struct { x: float32; y: float32; z: float32; }
Camera3D :: struct {
    position: Vector3
    target: Vector3
    up: Vector3
    fovy: float32
    projection: s32
}
ProbeClear :: (color: Color) #foreign probe_raylib "ClearBackground";
ProbeBegin3D :: (camera: Camera3D) #foreign probe_raylib "BeginMode3D";
ProbeEnd3D :: () #foreign probe_raylib "EndMode3D";
ProbeCube :: (position: Vector3, width: float32, height: float32, length: float32,
    color: Color) #foreign probe_raylib "DrawCube";

#program_export
main :: () -> s32 {
    if !OpenRaylib(640, 480) {
        CloseRaylib()
        return 1
    }
    session: Session = SessionOpen()
    camera: Camera3D
    camera.position = Vector3.{0.0, 0.0, 4.0}
    camera.target = Vector3.{0.0, 0.0, 0.0}
    camera.up = Vector3.{0.0, 1.0, 0.0}
    camera.fovy = 45.0
    while PollRaylib() {
        unused RaylibFrameInputBegin(session)
        BeginRaylibFrame()
        ProbeClear(Color.{140, 210, 240, 255})
        ProbeBegin3D(camera)
        ProbeCube(Vector3.{0.0, 0.0, 0.0}, 1.0, 1.0, 1.0, Color.{200, 80, 60, 255})
        ProbeEnd3D()
        viewport := RaylibViewport()
        TreePointerUpdate(session, PollPointer())
        if BeginFrame(session, cast(u64)1, viewport) == cast(FrameStatus)FrameOk {
            unused App.Frame(session, viewport)
            unused EndFrame(session)
        }
        FrameInputEnd(session)
        unused EndRaylibFrame()
    }
    CloseSession(session)
    CloseRaylib()
    return 0
}
'''


def run(command, cwd, timeout=1800):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=timeout)
    if result.returncode:
        raise AssertionError(f"{command!r} exited {result.returncode}\n"
                             f"stdout:\n{result.stdout[-6000:]}\nstderr:\n{result.stderr[-6000:]}")
    return result.stdout


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def chromium(url, profile, output, wait_ms=6000):
    """One private headless Chromium run with software WebGL and no desktop."""
    env = {key: value for key, value in os.environ.items() if key not in (
        "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER")}
    env["YUE_DESKTOP_RECOVERY"] = "0"
    # Chromium's singleton socket must fit a Unix socket path, so its
    # temporary directory stays short.
    socket_dir = tempfile.TemporaryDirectory(prefix="kr-", dir="/var/tmp")
    env["TMPDIR"] = socket_dir.name
    command = [os.environ.get("CHROMIUM", "chromium"), "--headless", "--no-sandbox",
               "--enable-unsafe-swiftshader", "--use-gl=angle", "--use-angle=swiftshader",
               "--enable-webgl", "--ignore-gpu-blocklist", "--disable-dev-shm-usage",
               "--no-first-run", "--no-default-browser-check", "--force-device-scale-factor=1",
               "--hide-scrollbars", "--window-size=640,480",
               # Real time: under virtual time the compositor never presents the
               # WebGL frames the page draws between its ASYNCIFY sleeps.
               f"--timeout={wait_ms}",
               "--user-data-dir=" + str(profile), *output, url]
    with socket_dir:
        result = subprocess.run(command, env=env, text=True, capture_output=True,
                                timeout=wait_ms / 1000 + 120)
    if result.returncode:
        raise AssertionError(f"Chromium exited {result.returncode}: {result.stderr[-4000:]}")
    return result.stdout


# The pixels the page shows once a frame is on screen: the 3D cube, the 3D
# clear color, the Kryon panel over it, and the asset image.
EXPECTED = (((320, 240), (200, 80, 60), "3D cube missing"),
            ((600, 440), (140, 210, 240), "3D clear missing"),
            ((100, 60), (30, 60, 200), "Kryon UI over 3D missing"),
            ((472, 52), (230, 30, 40), "asset image missing"))


def page_frame(url, project):
    """The page's pixels once a whole frame is on screen. A headless
    screenshot fires at one moment, and on a slow machine that can still show
    the WebGL canvas black before a frame is presented, so each attempt takes
    a fresh profile and waits longer."""
    for attempt, wait_ms in enumerate((6000, 20000, 45000)):
        shot = project / f"page-{attempt}.png"
        chromium(url, project / f"profile-shot-{attempt}", ["--screenshot=" + str(shot)], wait_ms)
        png = png_pixels(shot)
        assert png[:2] == (640, 480), png[:2]
        if all(rgb(png, x, y) == color for (x, y), color, _ in EXPECTED):
            break
    return png


def main():
    assert shutil.which(os.environ.get("CHROMIUM", "chromium")), "Install Chromium to test the page"
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="raylib-web-", dir=ROOT / "build") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "assets").mkdir()
        (project / "data/levels").mkdir(parents=True)
        (project / "ziran.toml").write_text(MANIFEST)
        (project / "ziran.local.toml").write_text(
            f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n'
            + (f'raylib = "{os.environ["RAYLIB_SOURCE"]}"\n' if os.environ.get("RAYLIB_SOURCE") else ""))
        (project / "src/app.zi").write_text(APP)
        (project / "src/host.zi").write_text(HOST)
        (project / "data/levels/probe.txt").write_text("kryon")
        solid_png(project / "assets/red.png", 64, 64, (230, 30, 40, 255))
        run([str(ZIRAN), "lock"], project)
        run([str(ZIRAN), "tool", "kryon", "build", "--profile", "web"], project)
        page = project / "build/raylib_web_probe-web"
        for name in ("index.html", "index.js", "index.wasm", "index.data"):
            assert (page / name).is_file(), f"page is missing {name}"

        handler = functools.partial(QuietHandler, directory=str(page))
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            url = f"http://127.0.0.1:{server.server_address[1]}/index.html"
            dom = chromium(url, project / "profile-dom", ["--dump-dom"])
            assert "data-kryon" not in dom, "page ended or aborted: " + dom[-2000:]
            title = re.search(r"<title>(.*?)</title>", dom, re.S)
            assert title and title.group(1) == "Raylib Web Probe", title and title.group(1)
            png = page_frame(url, project)
        finally:
            server.shutdown()
        for (x, y), color, failure in EXPECTED:
            assert rgb(png, x, y) == color, (failure, rgb(png, x, y))
    print("raylib web profile: 3D cube under Kryon UI, assets and preload mounted, page titled")


if __name__ == "__main__":
    main()
