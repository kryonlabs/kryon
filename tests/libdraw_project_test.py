"""Build and exercise the Ziran libdraw host on private Xvfb displays."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

from raylib_project_test import png_pixels, rgb, solid_png


ROOT = Path(__file__).resolve().parents[1]
PLAN9 = Path(os.environ.get("PLAN9PORT_DIR", ROOT.parent / "plan9port"))


def run(command, cwd, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(
            f"{command!r} exited {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        env.pop(name, None)
    env["PLAN9"] = str(PLAN9)
    env["DEVDRAW"] = str(PLAN9 / "bin/devdraw")
    env["PATH"] = f"{PLAN9 / 'bin'}:{env['PATH']}"
    return env


def window_probe(project, mode):
    env = os.environ.copy()
    assert env.get("KRYON_PRIVATE_XVFB") == "1"
    assert env.get("DISPLAY", "").startswith(":")
    assert int(env["DISPLAY"].split(".")[0][1:]) >= 100
    assert "WAYLAND_DISPLAY" not in env
    app = subprocess.Popen(
        [str(project / "build/libdraw_probe-libdraw")], cwd=project, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        window = None
        for _ in range(100):
            found = subprocess.run(
                ["xdotool", "search", "--name", "Kryon Ziran"],
                capture_output=True, text=True,
            )
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0]
                break
            if app.poll() is not None:
                raise AssertionError(f"libdraw exited early: {app.stderr.read()}")
            time.sleep(0.1)
        assert window, "libdraw window did not open"
        if mode == "pixels":
            time.sleep(0.3)
            xwd = project / "window.xwd"
            png = project / "window.png"
            run(["xwd", "-id", window, "-out", str(xwd)], project, env)
            run(["convert", str(xwd), "-type", "TrueColor",
                 f"PNG24:{png}"], project, env)
            pixels = png_pixels(png)
            assert rgb(pixels, 100, 90) == (230, 30, 40), "libdraw channel order failed"
            assert rgb(pixels, 10, 10) == (248, 250, 252), "libdraw background failed"
            assert rgb(pixels, 500, 130) == (30, 200, 40), "libdraw image failed"
            assert rgb(pixels, 701, 81) == (248, 250, 252), "rounded image clip failed"
            assert rgb(pixels, 780, 160) == (230, 30, 40), "rounded image center failed"
            return
        run(["xdotool", "windowfocus", window], project, env)
        if mode == "keyboard":
            run(["xdotool", "key", "a"], project, env)
        elif mode == "pointer":
            run(["xdotool", "mousemove", "--window", window,
                 "470", "432"], project, env)
            time.sleep(0.2)
            run(["xdotool", "mousedown", "1"], project, env)
            time.sleep(0.2)
            run(["xdotool", "mouseup", "1"], project, env)
        else:
            run(["xdotool", "windowsize", window, "800", "500"], project, env)
        stdout, stderr = app.communicate(timeout=10)
        assert app.returncode == 1, (mode, app.returncode, stdout, stderr)
        assert "libdraw frame rendering failed" in stderr, stderr
    finally:
        if app.poll() is None:
            app.terminate()
            app.communicate(timeout=5)


def main():
    for tool in ("xvfb-run", "xdotool", "xwd", "convert"):
        assert shutil.which(tool), f"Install {tool} to test the libdraw window"
    assert (PLAN9 / "lib/libdraw.a").exists(), "Build plan9port first"
    with tempfile.TemporaryDirectory(prefix="kryon-libdraw-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "kryon.toml").write_text(
            '[project]\nname = "libdraw_probe"\n'
            f'[paths]\nkryon = "{ROOT}"\nziran = "{ROOT.parent / "ziran"}"\n'
            '[profiles.libdraw]\nbackend = "libdraw"\n'
        )
        (project / "src/app.zi").write_text(
            '#import "button_props"\n#import "button_widget"\n'
            '#import "cairo_raster"\n#import "drawing_props"\n'
            '#import "geometry"\n#import "image_props"\n'
            '#import "image_widget"\n#import "session"\n'
            '#import "tree_input"\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    if KeyboardTake(session) == 97 { return 1 }\n'
            '    if viewport.width < 900.0 { return 1 }\n'
            '    RasterRoundedRectangle(Rectangle.{40.0, 40.0, 200.0, 100.0},\n'
            '        12.0, 0, Color.{230, 30, 40, 255})\n'
            '    RasterImage("red.png", cast(u32)0,\n'
            '        Rectangle.{0.0, 0.0, 32.0, 32.0},\n'
            '        Rectangle.{700.0, 80.0, 160.0, 160.0},\n'
            '        Rectangle.{700.0, 80.0, 160.0, 160.0},\n'
            '        Vector2.{0.0, 0.0}, 0.0, 32.0,\n'
            '        Color.{255, 255, 255, 255})\n'
            '    image: ImageProps\n'
            '    image.key = cast(u64)1\n'
            '    image.bounds = Rectangle.{450.0, 80.0, 160.0, 160.0}\n'
            '    image.asset_path = "green.png"\n'
            '    image.alt_text = "Green sample image"\n'
            '    Image(session, image)\n'
            '    button: ButtonProps\n'
            '    button.key = cast(u64)2\n'
            '    button.bounds = Rectangle.{350.0, 400.0, 240.0, 64.0}\n'
            '    button.label = "Press me"\n'
            '    if Button(session, button) > 0 { return 1 }\n'
            '    return 0\n}\n'
        )
        solid_png(project / "green.png", 32, 32, (30, 200, 40, 255))
        solid_png(project / "red.png", 32, 32, (230, 30, 40, 255))
        env = private_environment()
        run([str(ROOT / "build/bin/kryon"), "build", "--profile", "libdraw"],
            project, env)
        capture = project / "capture.png"
        env["KRYON_CAPTURE_PATH"] = str(capture)
        run(["xvfb-run", "-a", "-n", "100",
             str(ROOT / "build/bin/kryon"), "run", "--profile", "libdraw"],
            project, env)
        pixels = png_pixels(capture)
        assert pixels[0] > 900 and pixels[1] > 500, pixels[:2]
        assert rgb(pixels, 500, 130) == (30, 200, 40), "captured image failed"
        assert rgb(pixels, 10, 10) == (248, 250, 252), "captured background failed"
        for mode in ("pixels", "keyboard", "pointer", "resize"):
            probe_env = private_environment()
            probe_env["KRYON_PRIVATE_XVFB"] = "1"
            run(["xvfb-run", "-a", "-n", "100", sys.executable, __file__,
                 "--probe", str(project), mode], project, probe_env)
    print("libdraw Ziran project: window pixels, image, keyboard, pointer, and resize passed")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--probe":
        window_probe(Path(sys.argv[2]), sys.argv[3])
    else:
        main()
