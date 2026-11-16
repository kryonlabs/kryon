"""Build a Ziran raylib project and inspect its private-display capture."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
from image_test_tools import png_pixels, quadrant_png, rgb, run, solid_png


def main():
    assert shutil.which("xvfb-run"), "Install Xvfb to test the raylib window"
    with tempfile.TemporaryDirectory(prefix="kryon-raylib-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "raylib_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "raylib"\n\n'
            '[tool.kryon.profiles.raylib]\nbackend = "raylib"\n'
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
        (project / "src" / "app.zi").write_text(
            'Button :: #import "kryon/button";\n'
            'Image :: #import "kryon/image";\n'
            '#import "kryon/control_props"\n'
            '#import "kryon/geometry"\n'
            '#import "kryon/session"\n'
            '#import "kryon/style_sheet"\n'
            '#import "kryon/tree_input"\n'
            '#import "Zoom"\n'
            '#import "http_transfer"\n'
            '#import "net_http"\n'
            'using StyleField;\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    if KeyboardTake(session) == 65 {\n'
            '        return 1\n'
            '    }\n'
            '    if ZoomFactor(session) > 99.0 {\n'
            '        output: [64]u8\n'
            '        transfer: HttpTransfer\n'
            '        get := HttpRequest.{.method = "GET", .url = "http://127.0.0.1:9/"}\n'
            '        if HttpTransferStart(*transfer, get, output[:], 100) { return 2 }\n'
            '    }\n'
            '    typed: s32 = TypedCodepointTake(session)\n'
            '    if typed == 122 || typed == 233 { return 1 }\n'
            '    if PointerWheelTake(session, viewport) > 0.0 { return 1 }\n'
            '    if ZoomFactor(session) > 1.05 && ZoomFactor(session) < 1.5 { return 1 }\n'
            '    rules: StyleRules\n'
            '    rules.count = 1\n'
            '    rounded_style: StyleRule\n'
            '    rounded_style.selector = StyleDefaultSelector()\n'
            '    rounded_style.selector.kind = StyleKindImage()\n'
            '    rounded_style.selector.class_name = 7\n'
            '    rounded_style.style.fields = cast(u32)StyleRadius\n'
            '    rounded_style.style.radius = 32.0\n'
            '    rules.items[0] = rounded_style\n'
            '    InstallStyleRules(rules)\n'
            '    image: Image.Props\n'
            '    image.key = cast(u64)1\n'
            '    image.bounds = Rectangle.{80.0, 80.0, 240.0, 240.0}\n'
            '    image.asset_path = "test.png"\n'
            '    image.alt_text = "Red sample image"\n'
            '    Image.Draw(session, image)\n'
            '    cropped: Image.Props\n'
            '    cropped.key = cast(u64)2\n'
            '    cropped.bounds = Rectangle.{720.0, 80.0, 160.0, 160.0}\n'
            '    cropped.asset_path = "quadrants.png"\n'
            '    Image.Draw(session, cropped)\n'
            '    rounded: Image.Props\n'
            '    rounded.key = cast(u64)4\n'
            '    rounded.bounds = Rectangle.{450.0, 80.0, 240.0, 240.0}\n'
            '    rounded.asset_path = "test.png"\n'
            '    rounded.class_name = 7\n'
            '    Image.Draw(session, rounded)\n'
            '    button: Button.Props\n'
            '    button.key = cast(u64)3\n'
            '    button.bounds = Rectangle.{350.0, 400.0, 240.0, 64.0}\n'
            '    button.label = "Press me"\n'
            '    if Button.Draw(session, button) > 0 { return 1 }\n'
            '    return 0\n}\n'
        )
        solid_png(project / "test.png", 64, 64, (230, 30, 40, 255))
        quadrant_png(project / "quadrants.png")
        ziran = str(ZIRAN)
        run([ziran, "lock"], project)
        run([ziran, "tool", "kryon", "build", "--profile", "raylib"], project)
        capture = project / "capture.png"
        env = os.environ.copy()
        for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
            env.pop(name, None)
        env["KRYON_CAPTURE_PATH"] = str(capture)
        env["KRYON_FONT_PATH"] = str(ROOT / "assets/fonts/LiberationSans-Regular.ttf")
        run(["xvfb-run", "-a", "./build/raylib_probe-raylib"], project, env)
        png = png_pixels(capture)
        assert png[:2] == (960, 600), png[:2]
        assert rgb(png, 200, 200) == (230, 30, 40), "Image.Draw(Image.Props) asset failed"
        assert rgb(png, 760, 120) == (30, 200, 40), "second Image asset failed"
        assert rgb(png, 570, 200) == (230, 30, 40), "raylib image center failed"
        assert rgb(png, 451, 81) == (248, 250, 252), "rounded image corner leaked"
        assert rgb(png, 480, 85) == (230, 30, 40), "rounded image arc missing"
        # Zoom scales everything drawn: at 200% the 240 pixel image spans 480
        # screen pixels, and layout uses half the window, so the image that
        # began at 80 now begins at 160.
        zoomed = project / "zoomed.png"
        zoom_env = env.copy()
        zoom_env["KRYON_CAPTURE_PATH"] = str(zoomed)
        zoom_env["KRYON_ZOOM"] = "2"
        run(["xvfb-run", "-a", "./build/raylib_probe-raylib"], project, zoom_env)
        big = png_pixels(zoomed)
        assert big[:2] == (960, 600), big[:2]
        assert rgb(big, 120, 120) == (248, 250, 252), "zoom left the image unscaled"
        assert rgb(big, 400, 400) == (230, 30, 40), "zoomed image did not grow"
        assert rgb(big, 700, 400) == (248, 250, 252), "zoomed image grew too far"
        if shutil.which("xdotool"):
            (project / "input_check.py").write_text(
                "import os\nimport subprocess\nimport sys\nimport time\n"
                "env = os.environ.copy()\nenv.pop('KRYON_CAPTURE_PATH', None)\n"
                "app = subprocess.Popen(['./build/raylib_probe-raylib'], "
                "env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)\n"
                "try:\n"
                "    for _ in range(100):\n"
                "        found = subprocess.run(['xdotool', 'search', '--name', "
                "'^raylib_probe$'], capture_output=True, text=True)\n"
                "        if found.returncode == 0 and found.stdout.strip():\n"
                "            window = found.stdout.splitlines()[0]\n"
                "            break\n"
                "        if app.poll() is not None:\n"
                "            raise AssertionError('raylib window exited before key input')\n"
                "        time.sleep(0.1)\n"
                "    else:\n"
                "        raise AssertionError('raylib window did not appear')\n"
                "    subprocess.run(['xdotool', 'windowfocus', window], check=True)\n"
                "    if sys.argv[1] == 'key':\n"
                "        subprocess.run(['xdotool', 'key', 'a'], check=True)\n"
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
                "    elif sys.argv[1] == 'zoom':\n"
                "        subprocess.run(['xdotool', 'mousemove', '--window', "
                "window, '470', '432'], check=True)\n"
                "        time.sleep(0.2)\n"
                "        subprocess.run(['xdotool', 'keydown', 'ctrl'], check=True)\n"
                "        subprocess.run(['xdotool', 'click', '4'], check=True)\n"
                "        subprocess.run(['xdotool', 'keyup', 'ctrl'], check=True)\n"
                "    elif sys.argv[1] == 'pointer':\n"
                "        subprocess.run(['xdotool', 'mousemove', '--window', "
                "window, '470', '432'], check=True)\n"
                "        time.sleep(0.2)\n"
                "        subprocess.run(['xdotool', 'mousedown', '1'], check=True)\n"
                "        time.sleep(0.2)\n"
                "        subprocess.run(['xdotool', 'mouseup', '1'], check=True)\n"
                "    else:\n"
                "        subprocess.run(['xdotool', 'mousemove', '--window', "
                "window, '470', '432'], check=True)\n"
                "        time.sleep(0.2)\n"
                "        subprocess.run(['xdotool', 'click', '4'], check=True)\n"
                "    stdout, stderr = app.communicate(timeout=10)\n"
                "    assert app.returncode == 1, app.returncode\n"
                "    assert 'raylib frame rendering failed' in stderr, stderr\n"
                "finally:\n"
                "    if app.poll() is None:\n"
                "        app.terminate()\n"
                "        app.communicate(timeout=5)\n"
            )
            for mode in ("key", "text", "unicode", "pointer", "wheel", "zoom"):
                run(["xvfb-run", "-a", "python3", "input_check.py", mode],
                    project, env)
    print("raylib Ziran project: images, rounded clip, zoom, keyboard, Unicode text, pointer, and wheel passed")


if __name__ == "__main__":
    main()
