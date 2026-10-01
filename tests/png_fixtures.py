#!/usr/bin/env python3
"""Independent PNG/Adam7 fixtures and their exact RGBA reference pixels."""
import argparse
from pathlib import Path
import struct
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"
PASSES = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8),
          (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def paeth(a, b, c):
    p = a + b - c
    distances = [abs(p - value) for value in (a, b, c)]
    return (a, b, c)[distances.index(min(distances))]


def fixture(color, depth, interlace, width=13, height=11):
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    maximum = (1 << depth) - 1
    palette = [(20, 70, 240, 0), (90, 190, 30, 80), (220, 20, 60, 255), (255, 180, 20, 160)][:min(4, 1 << depth)]
    pixels, expected = [], bytearray()
    key = tuple((maximum // 3 + i * 31) & maximum for i in range(channels))
    for y in range(height):
        row = []
        for x in range(width):
            sample = tuple((x * 313 + y * 587 + i * 911) & maximum for i in range(channels))
            if x == 0 and y == 0:
                sample = key
            if color == 3:
                sample = ((x + y) % len(palette),)
                rgba = palette[sample[0]]
            else:
                converted = [value >> 8 if depth == 16 else value * 255 // maximum for value in sample]
                if color == 0:
                    rgba = (converted[0],) * 3 + (0 if sample == key else 255,)
                elif color == 2:
                    rgba = tuple(converted) + (0 if sample == key else 255,)
                elif color == 4:
                    rgba = (converted[0],) * 3 + (converted[1],)
                else:
                    rgba = tuple(converted)
            row.append(sample)
            expected.extend(rgba)
        pixels.append(row)
    raw = bytearray()
    stride = max(1, (channels * depth + 7) // 8)
    for start_x, start_y, step_x, step_y in PASSES if interlace else [(0, 0, 1, 1)]:
        xs, ys = range(start_x, width, step_x), range(start_y, height, step_y)
        if not xs or not ys:
            continue
        above = None
        for number, y in enumerate(ys):
            samples = [s for x in xs for s in pixels[y][x]]
            if depth == 16:
                row = b"".join(struct.pack(">H", s) for s in samples)
            elif depth == 8:
                row = bytes(samples)
            else:
                row = bytearray((len(samples) * depth + 7) // 8)
                for i, sample in enumerate(samples):
                    row[i * depth // 8] |= sample << (8 - depth - i * depth % 8)
            filter_kind = number % 5
            raw.append(filter_kind)
            for at, value in enumerate(row):
                a = row[at - stride] if at >= stride else 0
                b = above[at] if above is not None else 0
                c = above[at - stride] if above is not None and at >= stride else 0
                prediction = (0, a, b, (a + b) // 2, paeth(a, b, c))[filter_kind]
                raw.append((value - prediction) & 255)
            above = row
    header = struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, int(interlace))
    prefix = SIGNATURE + chunk(b"IHDR", header)
    if color == 3:
        prefix += chunk(b"PLTE", bytes(v for p in palette for v in p[:3]))
        prefix += chunk(b"tRNS", bytes(p[3] for p in palette))
    elif color in (0, 2):
        prefix += chunk(b"tRNS", b"".join(struct.pack(">H", s) for s in key))
    packed = zlib.compress(raw)
    split = len(packed) // 2
    png = prefix + chunk(b"IDAT", packed[:split]) + chunk(b"IDAT", packed[split:]) + chunk(b"IEND", b"")
    return png, bytes(raw), bytes(expected), width, height, packed


def generate(directory=None, zi=None):
    cases = [fixture(color, depth, interlace)
             for color, depths in [(0, [1, 2, 4, 8, 16]), (2, [8, 16]),
                                   (3, [1, 2, 4, 8]), (4, [8, 16]), (6, [8, 16])]
             for depth in depths for interlace in (False, True)]
    cases += [fixture(6, 8, True, 1, 1), fixture(3, 4, True, 1, 13), fixture(6, 16, True, 16, 1)]
    if directory:
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "cases.count").write_bytes(struct.pack(">I", len(cases)))
        # The image host must retain native Plan 9 files after PNG sniffing.
        header = f'{"r8g8b8a8":>11} {0:11} {0:11} {3:11} {2:11} '.encode("ascii")
        assert len(header) == 60
        (directory / "native.img").write_bytes(header + bytes([255, 60, 40, 220]) * 6)
        for i, (png, raw, rgba, width, height, packed) in enumerate(cases):
            (directory / f"case{i}.png").write_bytes(png)
            (directory / f"case{i}.rgba").write_bytes(struct.pack(">II", width, height) + rgba)
        png, raw, rgba, w, h, packed = cases[-3]
        header = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 1)
        def stream(value):
            return SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", value) + chunk(b"IEND", b"")
        corrupt = bytearray(packed); corrupt[-1] ^= 1
        invalid = [png[:-1], png + b"trailing", png[:29] + bytes([png[29] ^ 1]) + png[30:],
                   SIGNATURE + chunk(b"IHDR", header) + chunk(b"ABCD", b"") + png[33:],
                   stream(bytes(corrupt)), stream(packed + b"extra"),
                   stream(zlib.compress(raw + b"extra")), stream(zlib.compress(raw[:-1])),
                   stream(zlib.compress(bytes([5]) + raw[1:]))]
        (directory / "bad.count").write_bytes(struct.pack(">I", len(invalid)))
        for i, value in enumerate(invalid):
            (directory / f"bad{i}.png").write_bytes(value)
    if zi:
        lines = ['#import "png"', '#program_export', 'main :: () -> s32 {']
        for i in range(len(cases)):
            lines.append(f'    if Case{i}() != 0 {{ return {i + 1} }}')
        lines.extend(['    return 0', '}'])
        for i, (png, raw, rgba, width, height, packed) in enumerate(cases):
            lines.append(f'Case{i} :: () -> s32 {{')
            for name, data in [('file', png), ('raw', raw), ('expected', rgba), ('compressed', packed)]:
                lines.append(f'    {name}: [{len(data)}]u8 = .[' + ','.join(map(str, data)) + ']')
            lines.extend([
                '    info := InspectPNG(file[:])',
                f'    if !info.valid || info.width != {width} || info.height != {height} || info.scanlines != {len(raw)} {{ return 1 }}',
                f'    collected: [{len(packed)}]u8',
                '    if !CollectPNGData(file[:], collected[:]) { return 2 }',
                f'    for at: 0..{len(packed) - 1} {{ if collected[at] != compressed[at] {{ return 3 }} }}',
                f'    output: [{len(rgba)}]u8',
                '    if !DecodePNGScanlines(file[:], raw[:], output[:]) { return 4 }',
                f'    for at: 0..{len(rgba) - 1} {{ if output[at] != expected[at] {{ return 5 }} }}',
                '    return 0', '}'])
        zi.write_text('\n'.join(lines) + '\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--zi", type=Path)
    args = parser.parse_args()
    generate(args.directory, args.zi)
