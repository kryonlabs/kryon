#!/usr/bin/env python3
"""Checks the previews image_preview_behavior.zi wrote against Pillow."""
import sys
from pathlib import Path
from PIL import Image

work = Path(sys.argv[1])
COLORS = [(220, 30, 30), (30, 200, 40), (30, 40, 210), (250, 250, 250)]
# Pillow's turn for each Exif orientation, as ImageOps.exif_transpose.
TURNS = {
    2: Image.Transpose.FLIP_LEFT_RIGHT,
    3: Image.Transpose.ROTATE_180,
    4: Image.Transpose.FLIP_TOP_BOTTOM,
    5: Image.Transpose.TRANSPOSE,
    6: Image.Transpose.ROTATE_270,
    7: Image.Transpose.TRANSVERSE,
    8: Image.Transpose.ROTATE_90,
}


def quarters(width, height):
    image = Image.new('RGB', (width, height))
    for y in range(height):
        for x in range(width):
            index = (0 if y < height // 2 else 2) + (0 if x < width // 2 else 1)
            image.putpixel((x, y), COLORS[index])
    return image


def difference(left, right):
    total = sum(abs(p - q) for p, q in zip(left.tobytes(), right.tobytes()))
    return total / (left.width * left.height * 3)


def compare(name, expected, limit):
    path = work / name
    with Image.open(path) as image:
        if image.format != 'JPEG':
            sys.exit(f'{name} is {image.format}, not JPEG')
        if image.size != expected.size:
            sys.exit(f'{name} is {image.size}, expected {expected.size}')
        actual = image.convert('RGB')
        error = difference(actual, expected)
        if error > limit:
            sys.exit(f'{name} differs by {error:.1f} per channel')
        print(f'{name}: {image.size[0]}x{image.size[1]}, {error:.2f} per channel from Pillow')


source = quarters(48, 32)
for orientation in range(1, 9):
    expected = source.transpose(TURNS[orientation]) if orientation in TURNS else source
    compare(f'orientation-{orientation}.jpg', expected, 6)
compare('upright.jpg', source, 6)
large = quarters(640, 400)
compare('scaled.jpg', large.resize((64, 40), Image.Resampling.BOX), 8)
compare('scaled-turned.jpg', large.resize((64, 40), Image.Resampling.BOX).transpose(TURNS[8]), 8)
compare('compressed.jpg', large.resize((384, 240), Image.Resampling.BOX).transpose(TURNS[6]), 8)
alpha = Image.new('RGB', (32, 16))
for y in range(16):
    for x in range(32):
        alpha.putpixel((x, y), (255, 255, 255) if x < 16 else (127, 127, 127))
compare('alpha.jpg', alpha, 6)
gray = Image.new('RGB', (17, 9))
for y in range(9):
    for x in range(17):
        gray.putpixel((x, y), (x * 15,) * 3)
compare('gray.jpg', gray, 6)
