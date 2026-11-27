"""Exercise the Ziran SDL/Cairo host as an imported Kryon package."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib
import time

from image_test_tools import png_pixels, rgb, run, solid_png


from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")
DARK_BACKGROUND = (22, 25, 37)
LIGHT_BACKGROUND = (246, 245, 244)


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET", "XAUTHORITY",
                 "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER",
                 "XDG_SESSION_ID", "XDG_SESSION_TYPE", "DESKTOP_STARTUP_ID",
                 "SDL_MOUSE_FOCUS_CLICKTHROUGH", "GTK_THEME", "KRYON_TEST_DRIVER",
                 "THEME_PROBE_OVERRIDE", "HARMONY_APP_HOST_PID", "HARMONY_APP_CONTAINER"):
        env.pop(name, None)
    env.update(YUE_DESKTOP_RECOVERY="0", HARMONY_DESKTOP_RECOVERY="0")
    return env


def check_live_theme():
    assert os.environ.get("KRYON_PRIVATE_XVFB") == "1"
    env = os.environ.copy()
    env.pop("KRYON_CAPTURE_PATH", None)
    app = subprocess.Popen(["./build/desktop_probe-desktop"], env=env)
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            found = subprocess.run([
                "xdotool", "search", "--onlyvisible", "--pid", str(app.pid),
                "--name", "^desktop_probe$"], capture_output=True, text=True, timeout=3)
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0]
                break
            assert app.poll() is None, app.returncode
            time.sleep(0.05)
        else:
            raise AssertionError("desktop theme probe window did not appear")

        settings = Path(env["XDG_CONFIG_HOME"]) / "gtk-3.0/settings.ini"
        for name, expected in (("Fixture-Dark", DARK_BACKGROUND),
                               ("Fixture-Light", LIGHT_BACKGROUND)):
            pending = settings.with_suffix(".new")
            pending.write_text(f"[Settings]\ngtk-theme-name={name}\n")
            pending.replace(settings)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                subprocess.run(["import", "-window", window, "-crop", "1x1+10+10",
                                "PNG32:live-theme.png"], check=True, timeout=3)
                actual = rgb(png_pixels(Path("live-theme.png")), 0, 0)
                if actual == expected:
                    break
                time.sleep(0.05)
            else:
                raise AssertionError(f"{name} did not reach the live frame, got {actual}")
    finally:
        if app.poll() is None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
                app.wait()


def main():
    for tool in ("xvfb-run", "xdotool", "xprop", "xfwm4", "dbus-run-session", "xmessage", "import"):
        assert shutil.which(tool), f"Install {tool} to test the desktop window"
    with tempfile.TemporaryDirectory(prefix="kryon-desktop-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        toolchain = tomllib.loads((ROOT / "templates/app/ziran.toml").read_text())["toolchain"]
        (project / "ziran.toml").write_text(
            '[package]\nname = "desktop_probe"\nentry = "src/main.tsx"\n'
            'module_roots = ["src"]\nbridge_modules = ["main"]\n\n'
            f'[toolchain]\ngit = {json.dumps(toolchain["git"])}\n'
            f'ref = {json.dumps(toolchain["ref"])}\n\n'
            '[dependencies.kryon]\ngit = "https://git.waozi.xyz/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "desktop"\n\n'
            '[tool.kryon.profiles.desktop]\nbackend = "desktop"\n'
        )
        overrides = {
            "kryon": str(ROOT),
        }
        (project / "ziran.local.toml").write_text(
            "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        # main:main already emits C's process entry; a generated host would
        # collide with it. Other module names are covered by the examples.
        (project / "src/main.tsx").write_text('''import * as App from "kryon/app";
import { ScrollResult } from "kryon/scroll";
import { Session } from "kryon/session";
import { InstallDefaultTheme } from "kryon/Theme";
import { KeyboardControlTake, KeyboardTake, PointerWheelTake, SecondaryPointerSample,
    TypedCodepointTake } from "kryon/tree_input";
import { Image } from "kryon/tsx/image";
import { Scroll } from "kryon/tsx/scroll";
import { Text } from "kryon/tsx/text";
import { WindowQuit } from "kryon/window";
import { EnvironmentText } from "std/environment_linux";

let scrollOffset: s32 = s32(0);
let firstFrame: boolean = true;

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (firstFrame) {
        firstFrame = false;
        if (EnvironmentText("THEME_PROBE_OVERRIDE") === "light") { InstallDefaultTheme(false); }
    }
    if (KeyboardControlTake(session, s32(83)) || KeyboardTake(session) === s32(65)) {
        WindowQuit();
        return;
    }
    const typed = TypedCodepointTake(session);
    if (typed === s32(122) || typed === s32(233)) { WindowQuit(); return; }
    if (PointerWheelTake(session, frame.viewport) > float32(0)) { WindowQuit(); return; }
    if (SecondaryPointerSample(session).released) { WindowQuit(); return; }

    const scroll = Scroll({session: session, props: {key: u64(80n),
        bounds: {x: 700, y: 10, width: 200, height: 150}, content_height: s32(600),
        scroll_offset: scrollOffset}, children: (view: ScrollResult): void => {
        <Text session={session} props={{text: "End of scroll probe", bounds: {
            x: view.content.x, y: view.content.y + 450, width: view.content.width, height: 32}}}/>;
    }});
    scrollOffset = scroll.scroll_offset;
    if (scrollOffset > s32(0)) { WindowQuit(); return; }
    <Image session={session} props={{key: u64(1n), bounds: {x: 80, y: 80, width: 240, height: 240},
        asset_path: "red.png", alt_text: "Red image"}}/>;
    <Text session={session} props={{bounds: {x: 400, y: 100, width: 300, height: 200},
        text: "gjpqy", size: s32(40)}}/>;
}

export function main(): s32 { return App.Run({title: "desktop_probe"}, Frame); }
''')
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

        env.update(HOME=str(project / "home"), XDG_DATA_HOME=str(project / "data"))
        (project / "home").mkdir()
        settings = project / "config/gtk-3.0/settings.ini"
        settings.parent.mkdir(parents=True, exist_ok=True)
        for name, bg, fg in (("Fixture-Light", "f6f5f4", "2e3436"),
                             ("Fixture-Dark", "161925", "c3c7d1")):
            css = project / f"data/themes/{name}/gtk-3.0/gtk.css"
            css.parent.mkdir(parents=True)
            css.write_text(f"@define-color theme_bg_color #{bg};\n"
                           f"@define-color theme_fg_color #{fg};\n")
        settings.write_text("[Settings]\ngtk-theme-name=Fixture-Light\n")
        capture = project / "capture.png"
        env["KRYON_CAPTURE_PATH"] = str(capture)
        run(["xvfb-run", "-a", "./build/desktop_probe-desktop"], project, env)
        png = png_pixels(capture)
        assert png[:2] == (960, 600), png[:2]
        assert rgb(png, 200, 200) == (230, 30, 40), "desktop image failed"
        assert rgb(png, 10, 10) == LIGHT_BACKGROUND, ("light startup background", rgb(png, 10, 10))
        # A 40-pixel font's line box is 40 pixels tall, descenders included,
        # as on the desktop and canvas hosts.
        ink = [y for y in range(90, 300) if any(
            sum(rgb(png, x, y)) < 400 for x in range(400, 700))]
        assert ink, "desktop text failed"
        assert ink[0] >= 100 and ink[-1] < 140, (ink[0], ink[-1])
        assert ink[-1] >= 128, ("descenders missing", ink[-1])

        settings.write_text("[Settings]\ngtk-theme-name=Fixture-Dark\n")
        run(["xvfb-run", "-a", "./build/desktop_probe-desktop"], project, env)
        png = png_pixels(capture)
        assert rgb(png, 10, 10) == DARK_BACKGROUND, ("dark startup background", rgb(png, 10, 10))
        assert any(sum(rgb(png, x, y)) > 550 for x in range(400, 570)
                   for y in range(100, 140)), "dark system theme text is missing or unreadable"

        run(["xvfb-run", "-a", "./build/desktop_probe-desktop"], project,
            dict(env, THEME_PROBE_OVERRIDE="light"))
        png = png_pixels(capture)
        assert rgb(png, 10, 10) == (255, 255, 255), ("explicit first-frame background", rgb(png, 10, 10))
        assert any(sum(rgb(png, x, y)) < 400 for x in range(400, 570)
                   for y in range(100, 140)), "explicit first-frame text failed"

        run(["xvfb-run", "-a", "-s", "-screen 0 1280x1024x24 -nolisten tcp",
             "python3", str(Path(__file__).resolve()), "--theme-reload"],
            project, dict(env, KRYON_PRIVATE_XVFB="1"))

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
            "        found = subprocess.run(['xdotool', 'search', '--onlyvisible', "
            "'--pid', str(app.pid), '--name', "
            "'^desktop_probe$'], capture_output=True, text=True)\n"
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
            "    elif sys.argv[1] == 'scroll':\n"
            "        subprocess.run(['xdotool', 'mousemove', '--window', "
            "window, '780', '70', 'click', '5'], check=True)\n"
            "    stdout, stderr = app.communicate(timeout=10)\n"
            "    assert app.returncode == 0, (app.returncode, stdout, stderr)\n"
            "    assert 'desktop frame rendering failed' not in stderr, stderr\n"
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
        for mode in ("key", "control", "text", "unicode", "wheel", "scroll"):
            run(["xvfb-run", "-a", "python3", "input_check.py", mode],
                project, env)
        # A real window manager activates an unfocused window on the click.
        # Direct windowfocus above would hide SDL swallowing that first click.
        run(["xvfb-run", "-a", "dbus-run-session", "--", "python3",
             "input_check.py", "secondary"], project, env)
    print("desktop Ziran project: theme startup, override and reload, image, text line box, "
          "keyboard, modifiers, UTF-8 text, wheel, scroll widget, and first right-click passed")


if __name__ == "__main__":
    if sys.argv[1:] == ["--theme-reload"]:
        check_live_theme()
    else:
        main()
