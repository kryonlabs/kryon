"""Exercise the Ziran SDL/Cairo host as an imported Kryon package."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from raylib_project_test import png_pixels, rgb, run, solid_png


ROOT = Path(__file__).resolve().parents[1]
# The sibling checkout CI uses, else the org-grouped local layout.
ZIRAN_ROOT = Path(os.environ.get("ZIRAN_ROOT") or next(
    (path for path in (ROOT.parent / "ziran", ROOT.parent.parent / "ziranlang/ziran")
     if path.is_dir()), ROOT.parent / "ziran"))
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        env.pop(name, None)
    return env


def main():
    for tool in ("xvfb-run", "xdotool"):
        assert shutil.which(tool), f"Install {tool} to test the desktop window"
    with tempfile.TemporaryDirectory(prefix="kryon-desktop-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "desktop_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.Kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.Kryon]\ndefault_profile = "desktop"\n\n'
            '[tool.Kryon.profiles.desktop]\nbackend = "desktop"\n'
        )
        overrides = {
            "Kryon": str(ROOT),
            "ziran": str(ZIRAN_ROOT),
        }
        if os.environ.get("RAYLIB_SOURCE"):
            overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
        (project / "ziran.local.toml").write_text(
            "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        (project / "src/app.zi").write_text(
            'using UI :: #import "Widgets";\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    key: s32 = KeyboardTake(session)\n'
            '    if key == 65 { return 1 }\n'
            '    control: bool = (KeyboardModifiers(session) & KeyModifierControl) != 0\n'
            '    if key == 83 && control { return 1 }\n'
            '    typed: s32 = TypedCodepointTake(session)\n'
            '    if typed == 122 || typed == 233 { return 1 }\n'
            '    if PointerWheelTake(session, viewport) > 0.0 { return 1 }\n'
            '    image: ImageProps\n'
            '    image.key = cast(u64)1\n'
            '    image.bounds = Rectangle.{80.0, 80.0, 240.0, 240.0}\n'
            '    image.asset_path = "red.png"\n'
            '    image.alt_text = "Red image"\n'
            '    Image(session, image)\n'
            '    return 0\n}\n'
        )
        solid_png(project / "red.png", 32, 32, (230, 30, 40, 255))
        env = private_environment()
        run([ZIRAN, "lock"], project, env)
        run([ZIRAN, "tool", "Kryon", "build", "--profile", "desktop"],
            project, env)

        # The build describes the generated C to editors, one entry for each
        # file, with the flags it really compiles with.
        generated = project / "build/generated/desktop"
        commands = json.loads((generated / "compile_commands.json").read_text())
        sources = sorted(path.name for path in (generated / "c").glob("*.c"))
        assert sources, "no generated C"
        assert [Path(entry["file"]).name for entry in commands] == sources
        for entry in commands:
            arguments = entry["arguments"]
            assert entry["directory"] == str(project.resolve()), entry["directory"]
            assert Path(arguments[0]).is_absolute(), arguments[0]
            assert "-std=c99" in arguments and "-pedantic-errors" in arguments
            assert f"-I{(generated / 'c').resolve()}" in arguments
            assert arguments[-2:] == ["-c", entry["file"]], arguments[-2:]

        capture = project / "capture.png"
        env["KRYON_CAPTURE_PATH"] = str(capture)
        run(["xvfb-run", "-a", "./build/desktop_probe-desktop"], project, env)
        png = png_pixels(capture)
        assert png[:2] == (960, 600), png[:2]
        assert rgb(png, 200, 200) == (230, 30, 40), "desktop image failed"
        assert rgb(png, 10, 10) == (248, 250, 252), "desktop background failed"

        (project / "input_check.py").write_text(
            "import os\nimport subprocess\nimport sys\nimport time\n"
            "env = os.environ.copy()\nenv.pop('KRYON_CAPTURE_PATH', None)\n"
            "app = subprocess.Popen(['./build/desktop_probe-desktop'], "
            "env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)\n"
            "try:\n"
            "    for _ in range(100):\n"
            "        found = subprocess.run(['xdotool', 'search', '--name', "
            "'Kryon Ziran'], capture_output=True, text=True)\n"
            "        if found.returncode == 0 and found.stdout.strip():\n"
            "            window = found.stdout.splitlines()[0]\n"
            "            break\n"
            "        if app.poll() is not None:\n"
            "            raise AssertionError('desktop window exited early')\n"
            "        time.sleep(0.1)\n"
            "    else:\n"
            "        raise AssertionError('desktop window did not appear')\n"
            "    subprocess.run(['xdotool', 'windowfocus', window], check=True)\n"
            "    if sys.argv[1] == 'key':\n"
            "        subprocess.run(['xdotool', 'key', 'a'], check=True)\n"
            "    elif sys.argv[1] == 'control':\n"
            "        subprocess.run(['xdotool', 'key', 'ctrl+s'], check=True)\n"
            "    elif sys.argv[1] == 'text':\n"
            "        subprocess.run(['xdotool', 'key', 'z'], check=True)\n"
            "    elif sys.argv[1] == 'unicode':\n"
            "        # The default Xvfb keymap has no e-acute, so xdotool maps it to a\n"
            "        # spare keycode and restores the map right after the press. A\n"
            "        # toolkit that reads the press after the restore sees no text;\n"
            "        # press again until the window takes it.\n"
            "        for _ in range(20):\n"
            "            subprocess.run(['xdotool', 'key', 'eacute'], check=True)\n"
            "            try:\n"
            "                app.wait(timeout=0.5)\n"
            "                break\n"
            "            except subprocess.TimeoutExpired:\n"
            "                pass\n"
            "    else:\n"
            "        subprocess.run(['xdotool', 'mousemove', '--window', "
            "window, '470', '432'], check=True)\n"
            "        time.sleep(0.2)\n"
            "        subprocess.run(['xdotool', 'click', '4'], check=True)\n"
            "    stdout, stderr = app.communicate(timeout=10)\n"
            "    assert app.returncode == 1, (app.returncode, stdout, stderr)\n"
            "    assert 'desktop frame rendering failed' in stderr, stderr\n"
            "finally:\n"
            "    if app.poll() is None:\n"
            "        app.terminate()\n"
            "        app.communicate(timeout=5)\n"
        )
        for mode in ("key", "control", "text", "unicode", "wheel"):
            run(["xvfb-run", "-a", "python3", "input_check.py", mode],
                project, env)
    print("desktop Ziran project: image, keyboard, modifiers, UTF-8 text, and wheel passed")


if __name__ == "__main__":
    main()
