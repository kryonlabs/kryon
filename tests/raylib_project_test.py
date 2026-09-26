"""Build a Ziran raylib project and inspect its private-display capture."""

import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib


ROOT = Path(__file__).resolve().parents[1]
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def solid_png(path, width, height, color):
    scan = b"".join(b"\x00" + bytes(color) * width for _ in range(height))
    path.write_bytes(
        PNG_SIGNATURE
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scan))
        + chunk(b"IEND", b"")
    )


def quadrant_png(path):
    scan = bytearray()
    colors = ((30, 200, 40, 255), (230, 30, 40, 255),
              (30, 40, 220, 255), (230, 200, 40, 255))
    for y in range(64):
        scan.append(0)
        for x in range(64):
            scan.extend(colors[(y >= 32) * 2 + (x >= 32)])
    path.write_bytes(
        PNG_SIGNATURE
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 64, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scan))
        + chunk(b"IEND", b"")
    )


def png_pixels(path):
    data = path.read_bytes()
    assert data.startswith(PNG_SIGNATURE)
    offset = len(PNG_SIGNATURE)
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            assert depth == 8 and color in (2, 6) and interlace == 0
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(compressed)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_kind = raw[offset]
        row = bytearray(raw[offset + 1 : offset + 1 + stride])
        offset += 1 + stride
        assert filter_kind in range(5)
        for col in range(stride):
            left = row[col - channels] if col >= channels else 0
            above = previous[col]
            upper_left = previous[col - channels] if col >= channels else 0
            if filter_kind == 1:
                row[col] = (row[col] + left) & 255
            elif filter_kind == 2:
                row[col] = (row[col] + above) & 255
            elif filter_kind == 3:
                row[col] = (row[col] + (left + above) // 2) & 255
            elif filter_kind == 4:
                estimate = left + above - upper_left
                distances = [abs(estimate - n) for n in (left, above, upper_left)]
                predictor = (left, above, upper_left)[distances.index(min(distances))]
                row[col] = (row[col] + predictor) & 255
        rows.append(row)
        previous = row
    return width, height, channels, rows


def rgb(png, x, y):
    _, _, channels, rows = png
    return tuple(rows[y][x * channels : x * channels + 3])


def run(command, cwd, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(
            f"{command!r} exited {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )


def main():
    assert shutil.which("xvfb-run"), "Install Xvfb to test the raylib window"
    with tempfile.TemporaryDirectory(prefix="kryon-raylib-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "kryon.toml").write_text(
            f'[project]\nname = "raylib_probe"\n'
            f'[paths]\nkryon = "{ROOT}"\nziran = "{ROOT.parent / "ziran"}"\n'
            '[profiles.raylib]\nbackend = "raylib"\n'
        )
        (project / "src" / "app.zi").write_text(
            '#import "button_props"\n#import "button_widget"\n'
            '#import "drawing_props"\n#import "geometry"\n'
            '#import "image_props"\n#import "image_widget"\n'
            '#import "raylib_game"\n'
            '#import "raylib_runtime"\n#import "session"\n'
            '#import "tree_input"\n'
            'sample_texture: Texture2D;\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    if KeyboardTake(session) == 65 {\n'
            '        return 1\n'
            '    }\n'
            '    if RaylibTypedCodepoint() == 122 { return 1 }\n'
            '    if RaylibWheelMove() > 0.0 { return 1 }\n'
            '    if sample_texture.id == cast(u32)0 {\n'
            '        sample_texture = LoadTexture("quadrants.png")\n'
            '    }\n'
            '    image: ImageProps\n'
            '    image.key = cast(u64)1\n'
            '    image.bounds = Rectangle.{80.0, 80.0, 240.0, 240.0}\n'
            '    image.asset_path = "test.png"\n'
            '    image.alt_text = "Red sample image"\n'
            '    Image(session, image)\n'
            '    cropped: ImageProps\n'
            '    cropped.key = cast(u64)2\n'
            '    cropped.bounds = Rectangle.{720.0, 80.0, 160.0, 160.0}\n'
            '    cropped.source = Rectangle.{0.0, 0.0, 32.0, 32.0}\n'
            '    cropped.asset_path = "test.png"\n'
            '    cropped.texture = sample_texture\n'
            '    Image(session, cropped)\n'
            '    RasterImage("test.png", cast(u32)0,\n'
            '        Rectangle.{0.0, 0.0, 64.0, 64.0},\n'
            '        Rectangle.{450.0, 80.0, 240.0, 240.0},\n'
            '        Rectangle.{450.0, 80.0, 240.0, 240.0},\n'
            '        Vector2.{0.0, 0.0}, 0.0, 32.0,\n'
            '        Color.{255, 255, 255, 255})\n'
            '    button: ButtonProps\n'
            '    button.key = cast(u64)3\n'
            '    button.bounds = Rectangle.{350.0, 400.0, 240.0, 64.0}\n'
            '    button.label = "Press me"\n'
            '    if Button(session, button) > 0 { return 1 }\n'
            '    return 0\n}\n'
        )
        solid_png(project / "test.png", 64, 64, (230, 30, 40, 255))
        quadrant_png(project / "quadrants.png")
        run([str(ROOT / "build/bin/kryon"), "build", "--profile", "raylib"], project)
        capture = project / "capture.png"
        env = os.environ.copy()
        for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
            env.pop(name, None)
        env["KRYON_CAPTURE_PATH"] = str(capture)
        env["KRYON_FONT_PATH"] = str(ROOT / "assets/fonts/LiberationSans-Regular.ttf")
        run(["xvfb-run", "-a", "./build/raylib_probe-raylib"], project, env)
        png = png_pixels(capture)
        assert png[:2] == (960, 600), png[:2]
        assert rgb(png, 200, 200) == (230, 30, 40), "Image(ImageProps) asset failed"
        assert rgb(png, 800, 160) == (30, 200, 40), "cropped Texture2D UVs failed"
        assert rgb(png, 570, 200) == (230, 30, 40), "raylib image center failed"
        assert rgb(png, 451, 81) == (248, 250, 252), "rounded image corner leaked"
        assert rgb(png, 480, 85) == (230, 30, 40), "rounded image arc missing"
        if shutil.which("xdotool"):
            (project / "input_check.py").write_text(
                "import os\nimport subprocess\nimport sys\nimport time\n"
                "env = os.environ.copy()\nenv.pop('KRYON_CAPTURE_PATH', None)\n"
                "app = subprocess.Popen(['./build/raylib_probe-raylib'], "
                "env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)\n"
                "try:\n"
                "    for _ in range(100):\n"
                "        found = subprocess.run(['xdotool', 'search', '--name', "
                "'Kryon Ziran'], capture_output=True, text=True)\n"
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
            for mode in ("key", "text", "pointer", "wheel"):
                run(["xvfb-run", "-a", "python3", "input_check.py", mode],
                    project, env)
    print("raylib Ziran project: images, rounded clip, keyboard, text, pointer, and wheel passed")


if __name__ == "__main__":
    main()
