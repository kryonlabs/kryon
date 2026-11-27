"""Render the dashboard and exercise a resized ZX player on private Xvfb."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

from image_test_tools import png_pixels, rgb
from toolchain import ROOT, ZIRAN

APP = ROOT / "examples/dashboard"
OUTPUT = ROOT / "build/scratch/dashboard-responsive"
PLAYER = ROOT / "build/bin/zx"
PREVIEW = OUTPUT / "preview"
BUNDLE = OUTPUT / "responsive.zx"
HEIGHT = 840
WIDTHS = (550, 1200)


def run(*args, **options):
    return subprocess.run(args, check=True, timeout=180, **options)


def nodes(width):
    tree = json.loads(Path(f"{PREVIEW}-{width}.json").read_text())
    assert (tree["width"], tree["height"]) == (width, HEIGHT)
    return {node["key"]: node for node in tree["nodes"][1:]}


def check_previews():
    narrow, wide = (nodes(width) for width in WIDTHS)
    assert narrow[106]["y"] > narrow[100]["y"]
    assert len({wide[key]["y"] for key in (100, 103, 106, 109)}) == 1
    assert narrow[206]["x"] == narrow[203]["x"] and narrow[206]["y"] > narrow[203]["y"]
    assert wide[206]["x"] > wide[203]["x"] and wide[206]["y"] == wide[203]["y"]
    assert narrow[4]["width"] < wide[4]["width"], "headline should grow at the breakpoint"
    assert narrow[303]["visible_width"] < narrow[303]["width"]
    assert wide[303]["visible_width"] == wide[303]["width"]
    for width, tree, padding in ((550, narrow, 12), (1200, wide, 20)):
        assert tree[200]["width"] == tree[30]["width"]
        assert tree[201]["x"] - tree[200]["x"] == padding
        assert tree[201]["y"] - tree[200]["y"] == padding
        image = png_pixels(Path(f"{PREVIEW}-{width}.png"))
        assert rgb(image, tree[200]["x"] + 45, tree[200]["y"] + 4) == (21, 31, 48)
        assert tree[43]["visible"] and tree[43]["interactive"]


def check_input():
    with tempfile.TemporaryFile(dir=OUTPUT) as log:
        player = subprocess.Popen(["stdbuf", "-oL", PLAYER, BUNDLE], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 25
            window = None
            while player.poll() is None and time.monotonic() < deadline:
                found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid",
                                        str(player.pid)], capture_output=True, text=True, timeout=5)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[0]
                    break
                time.sleep(0.1)
            assert window, os.pread(log.fileno(), 4096, 0)
            run("xdotool", "windowfocus", "--sync", window)
            for width in WIDTHS:
                run("xdotool", "windowsize", "--sync", window, str(width), str(HEIGHT))
                button = nodes(width)[43]
                time.sleep(0.5)
                run("xdotool", "mousemove", "--window", window, str(button["x"] + 20),
                    str(button["y"] + 16), "mousedown", "1")
                time.sleep(0.4)
                run("xdotool", "mouseup", "1")
                marker = f"dashboard input passed at width {width}"
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline and player.poll() is None:
                    if marker.encode() in os.pread(log.fileno(), 4096, 0):
                        break
                    time.sleep(0.1)
                else:
                    output = os.pread(log.fileno(), 4096, 0).decode(errors="replace")
                    raise AssertionError(f"{marker!r} missing from player output: {output!r}")
        finally:
            if player.poll() is None:
                player.terminate()
                player.wait(timeout=10)


def main():
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                "SESSION_MANAGER", "DESKTOP_SESSION", "ICEAUTHORITY", "XDG_SESSION_TYPE",
                "XDG_SESSION_ID", "SDL_VIDEODRIVER"):
        env.pop(key, None)
    env.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0",
               XDG_CONFIG_HOME=str(OUTPUT / "config"))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    run(ROOT / "build/bin/kryon", "preview", "responsive", "--theme", "dark", "--width", "550,1200",
        "--height", str(HEIGHT), "--output", PREVIEW, cwd=APP, env=env)
    check_previews()
    run(ZIRAN, "bundle", "--project", "--define", "KRYON_ZX", "--entry", "responsive:main",
        "-o", BUNDLE, "previews/responsive.tsx", cwd=APP, env=env)
    run(PLAYER, "--check", BUNDLE, env=env)
    env["SDL_VIDEODRIVER"] = "x11"
    run("xvfb-run", "-a", "-s", "-screen 0 1400x1000x24", sys.executable, __file__, "--input", env=env)
    print("responsive dashboard geometry, paint and ZX input at 550 and 1200: PASS")


if __name__ == "__main__":
    check_input() if sys.argv[1:] == ["--input"] else main()
