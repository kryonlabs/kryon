"""Pillow goldens for smooth images drawn through Lanczos copies.

    lanczos_goldens.py fixtures DIR        write opaque and alpha .png and .argb
    lanczos_goldens.py compare DIR OUTPUT  check each frame OUTPUT names

Each OUTPUT line `BACKEND KIND INDEX X Y WIDTH HEIGHT` names a 128 by 128
frame DIR/KIND-INDEX.bgra (Cairo, pixmap) or .rgba (Raylib, Canvas), drawn over the background
(248, 250, 252) and clipped to (7, 9, 113, 103), whose picture covers the
device pixels X, Y, WIDTH, HEIGHT. The expected copy is Pillow's
Image.resize(LANCZOS, box) of the premultiplied picture (Cairo) or of the
picture itself, which Pillow unpremultiplies as the straight-alpha hosts
(pixmap, Raylib, Canvas) do, composited with the case's tint alpha. Opaque
pictures under an opaque tint must match exactly; blending a translucent copy
may differ by the hosts' rounding. A browser also stores canvases
premultiplied in 8 bits and blends with its own rounding (Firefox: about +0.6
on translucent pictures, -0.7 under a half alpha tint), so Canvas blends get a
unit more.
"""
import sys
from pathlib import Path

from PIL import Image

BOXES = [(0.0, 0.0, 512.0, 512.0), (37.5, 20.25, 300.0, 280.5),
         (100.0, 100.0, 24.0, 18.0), (0.0, 0.0, 512.0, 512.0)]
TINT_ALPHA = [255, 255, 255, 128]
CLIP = (7, 9, 7 + 113, 9 + 103)
SIZE = 128
BACKGROUND = (248, 250, 252)


def fixtures(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    pixels = []
    for y in range(512):
        for x in range(512):
            # Smooth shading, a fine checker that rings and hard bars.
            r = (x * 255) // 511
            g = (y * 255) // 511
            b = 255 if (x // 3 + y // 3) % 2 == 0 else 0
            if 200 <= x < 210 or 300 <= y < 304:
                r, g, b = 255, 255, 255
            pixels.append((r, g, b))
    opaque = Image.new('RGB', (512, 512))
    opaque.putdata(pixels)
    opaque.convert('RGBA').save(directory / 'opaque.png')
    alpha = opaque.convert('RGBA')
    alpha.putalpha(Image.linear_gradient('L').resize((512, 512)).rotate(90))
    alpha.save(directory / 'alpha.png')
    # The pixmap host's 0xAARRGGBB words, little endian.
    for name, picture in (('opaque', opaque.convert('RGBA')), ('alpha', alpha)):
        (directory / f'{name}.argb').write_bytes(picture.tobytes('raw', 'BGRA'))


def expected_frame(directory, kind, index, left, top, width, height, straight):
    picture = Image.open(Path(directory) / f'{kind}.png').convert('RGBA')
    x, y, w, h = BOXES[index]
    if not straight:
        picture = picture.convert('RGBa')
    copy = picture.resize((width, height), Image.LANCZOS, box=(x, y, x + w, y + h))
    data = copy.tobytes()
    tint = TINT_ALPHA[index] / 255.0
    frame = []
    for py in range(SIZE):
        for px in range(SIZE):
            color = BACKGROUND
            inside = (CLIP[0] <= px < CLIP[2] and CLIP[1] <= py < CLIP[3] and
                      left <= px < left + width and top <= py < top + height)
            if inside:
                at = ((py - top) * width + (px - left)) * 4
                r, g, b, a = data[at:at + 4]
                if straight:
                    r, g, b = (c * a / 255.0 for c in (r, g, b))
                keep = 1.0 - a * tint / 255.0
                # Ringing can lift a premultiplied color past its alpha;
                # compositing saturates it, as pixman and GPUs do.
                color = tuple(min(255.0, c * tint + base * keep) for c, base in zip((r, g, b), BACKGROUND))
            frame.append(color)
    return frame


def compare(directory, output):
    failures = 0
    cases = 0
    for line in Path(output).read_text().splitlines():
        fields = line.split()
        if len(fields) != 7 or fields[0] not in ('cairo', 'raylib', 'pixmap', 'canvas'):
            continue
        backend, kind, index = fields[0], fields[1], int(fields[2])
        left, top, width, height = (int(float(value)) for value in fields[3:])
        suffix = 'bgra' if backend in ('cairo', 'pixmap') else 'rgba'
        actual = (Path(directory) / f'{kind}-{index}.{suffix}').read_bytes()
        if len(actual) != SIZE * SIZE * 4:
            print(f'{backend} {kind} {index}: frame has {len(actual)} bytes')
            failures += 1
            continue
        expected = expected_frame(directory, kind, index, left, top, width, height,
                                  backend in ('pixmap', 'raylib', 'canvas'))
        exact = kind == 'opaque' and TINT_ALPHA[index] == 255
        worst = 0
        for at, color in enumerate(expected):
            pixel = actual[at * 4:at * 4 + 3]
            if suffix == 'bgra':
                pixel = pixel[::-1]
            for value, want in zip(pixel, color):
                worst = max(worst, abs(value - want))
        limit = 0.5 if exact else (3.5 if backend == 'canvas' else 2.5)
        cases += 1
        status = 'ok' if worst <= limit else 'FAIL'
        print(f'{backend} {kind} {index}: largest difference from Pillow {worst:.1f} ({status})')
        if worst > limit:
            failures += 1
    if cases == 0:
        sys.exit('no frames to compare')
    if failures:
        sys.exit(f'{failures} of {cases} frames differ from Pillow')


if __name__ == '__main__':
    if sys.argv[1] == 'fixtures':
        fixtures(sys.argv[2])
    elif sys.argv[1] == 'compare':
        compare(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
