"""Exercise the Ziran SDL/Cairo host as an imported Kryon package."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from raylib_project_test import png_pixels, rgb, run, solid_png


from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                 "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER",
                 "SDL_MOUSE_FOCUS_CLICKTHROUGH"):
        env.pop(name, None)
    return env


def main():
    for tool in ("xvfb-run", "xdotool", "xprop", "xfwm4", "dbus-run-session", "xmessage"):
        assert shutil.which(tool), f"Install {tool} to test the desktop window"
    with tempfile.TemporaryDirectory(prefix="kryon-desktop-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "desktop_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "desktop"\n\n'
            '[tool.kryon.profiles.desktop]\nbackend = "desktop"\n'
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
            '    if SecondaryPointerSample(session).released { return 1 }\n'
            '    image: ImageProps\n'
            '    image.key = cast(u64)1\n'
            '    image.bounds = Rectangle.{80.0, 80.0, 240.0, 240.0}\n'
            '    image.asset_path = "red.png"\n'
            '    image.alt_text = "Red image"\n'
            '    Image(session, image)\n'
            '    label: TextProps\n'
            '    label.bounds = Rectangle.{400.0, 100.0, 300.0, 200.0}\n'
            '    label.text = "gjpqy"\n'
            '    label.font = 40\n'
            '    Text(session, label)\n'
            '    return 0\n}\n'
        )
        solid_png(project / "red.png", 32, 32, (230, 30, 40, 255))
        env = private_environment()
        env["XDG_CONFIG_HOME"] = str(project / "config")
        run([ZIRAN, "lock"], project, env)
        run([ZIRAN, "tool", "kryon", "build", "--profile", "desktop"],
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
        # A 40-pixel font's line box is 40 pixels tall, descenders included,
        # as on the raylib and canvas hosts.
        ink = [y for y in range(90, 300) if any(
            sum(rgb(png, x, y)) < 400 for x in range(400, 700))]
        assert ink, "desktop text failed"
        assert ink[0] >= 100 and ink[-1] < 140, (ink[0], ink[-1])
        assert ink[-1] >= 128, ("descenders missing", ink[-1])

        (project / "input_check.py").write_text(
            "import os\nimport subprocess\nimport sys\nimport time\n"
            "env = os.environ.copy()\nenv.pop('KRYON_CAPTURE_PATH', None)\n"
            "manager = None\nother = None\n"
            "if sys.argv[1] == 'secondary':\n"
            "    manager = subprocess.Popen(['xfwm4', '--sm-client-disable', "
            "'--compositor=off'], env=env, stdout=subprocess.PIPE, "
            "stderr=subprocess.STDOUT, text=True)\n"
            "    for _ in range(100):\n"
            "        if manager.poll() is not None:\n"
            "            raise AssertionError(('window manager exited', "
            "manager.returncode, manager.communicate()[0]))\n"
            "        supported = subprocess.run(['xprop', '-root', "
            "'_NET_SUPPORTED'], capture_output=True, text=True)\n"
            "        if supported.returncode == 0 and "
            "'_NET_ACTIVE_WINDOW' in supported.stdout:\n"
            "            break\n"
            "        time.sleep(0.1)\n"
            "    else:\n"
            "        raise AssertionError('private window manager did not become ready')\n"
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
            "    if sys.argv[1] == 'secondary':\n"
            "        other = subprocess.Popen(['xmessage', '-title', "
            "'KryonFocusProbe', '-geometry', '180x70+1050+900', "
            "'Private focus test'], env=env, stdout=subprocess.DEVNULL, "
            "stderr=subprocess.DEVNULL)\n"
            "        for _ in range(100):\n"
            "            found = subprocess.run(['xdotool', 'search', '--onlyvisible', "
            "'--name', '^KryonFocusProbe$'], capture_output=True, text=True)\n"
            "            if found.returncode == 0 and found.stdout.strip():\n"
            "                other_window = found.stdout.splitlines()[0]\n"
            "                break\n"
            "            time.sleep(0.1)\n"
            "        else:\n"
            "            raise AssertionError('focus helper window did not appear')\n"
            "        subprocess.run(['xdotool', 'windowactivate', '--sync', "
            "other_window], check=True)\n"
            "        subprocess.run(['xdotool', 'mousemove', '--window', "
            "window, '470', '432'], check=True)\n"
            "        time.sleep(0.2)\n"
            "        subprocess.run(['xdotool', 'click', '3'], check=True)\n"
            "    else:\n"
            "        subprocess.run(['xdotool', 'windowfocus', window], check=True)\n"
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
            "    elif sys.argv[1] == 'wheel':\n"
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
            "    for child in (other, manager):\n"
            "        if child is not None:\n"
            "            child.terminate()\n"
            "            try:\n"
            "                child.wait(timeout=5)\n"
            "            except subprocess.TimeoutExpired:\n"
            "                child.kill()\n"
            "                child.wait()\n"
        )
        for mode in ("key", "control", "text", "unicode", "wheel"):
            run(["xvfb-run", "-a", "python3", "input_check.py", mode],
                project, env)
        # A real window manager activates an unfocused window on the click.
        # Direct windowfocus above would hide SDL swallowing that first click.
        run(["xvfb-run", "-a", "dbus-run-session", "--", "python3",
             "input_check.py", "secondary"], project, env)
    print("desktop Ziran project: image, text line box, keyboard, modifiers, UTF-8 text, wheel, and first right-click passed")


if __name__ == "__main__":
    main()
