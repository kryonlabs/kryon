#!/usr/bin/env python3
"""Generate 16x16 pixel-art RGBA PNGs for FreeBSD and macOS platform icons.

The existing kryon platform icons (tux/droid/win/wasm) are flat, sharp
pixel-art with transparent backgrounds. This hand-crafts two more in that
style using raw RGBA pixel grids and encodes them as PNGs via the stdlib
(no PIL dependency).
"""
import struct
import zlib

TRANSPARENT = (0, 0, 0, 0)


def encode_png(path, pixels, width, height):
    """Encode a width*height RGBA pixel list (row-major) into a PNG file."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0 (None) per scanline
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            raw += bytes((r, g, b, a))

    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit, colour type 6 (RGBA)
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def grid(rows):
    """Build a flat pixel list from an array of strings. Each char maps to a color.

    ' ' or '.' = transparent. Other chars are looked up in the palette passed
    via a closure: we return (palette, pixels).
    """
    width = max(len(r) for r in rows)
    out = []
    for row in rows:
        row = row.ljust(width)
        out.append(row)
    return out, width


# ---- FreeBSD: a stylized red "beastie" devil head, horns + eyes ----
# Palette
R = (205, 45, 45, 255)      # beastie red body
RD = (155, 28, 28, 255)     # darker red shade
W = (245, 245, 245, 255)    # eye white
K = (25, 18, 18, 255)       # outline / pupil

# 16x16. Bold horned silhouette with a pointed chin and two clear eyes.
# Horns sweep up-and-out, head fills the middle, chin tapers at bottom.
freebsd = [
    "................",
    "...RR......RR...",
    "..RRRR....RRRR..",
    "..RRRRR..RRRRR..",
    ".RRRRRRRRRRRRRR.",
    ".RRRRRRRRRRRRRR.",
    "RRRRRRRRRRRRRRRR",
    "RRWWRRRRRRRRWWRR",
    "RRWWRRRRRRRRWWRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    ".RRRRRRRRRRRRRR.",
    ".RRRRRRRRRRRRRR.",
    "..RRRRRRRRRRRR..",
    "...RRRRRRRRRR...",
    ".....RRRRRR.....",
]

# ---- macOS: a graphite apple silhouette with a bite + leaf ----
G = (58, 58, 62, 255)       # graphite body
GD = (35, 35, 40, 255)      # deeper shade
L = (110, 170, 75, 255)     # leaf green

# 16x16. Classic apple: notch at top center where the leaf stem sits,
# leaf leaning right from the notch, bite notch on the upper-right side.
macos = [
    "................",
    "......LLL.......",
    ".....LL.........",
    "................",
    "....GGGGGGG.....",
    "...GGGGGGGGG.G..",
    "..GGGGGGGGGGGG..",
    ".GGGGGGGGGGGGG..",
    ".GGGGGGGGGGGGG..",
    ".GGGGGGGGGGGGG..",
    ".GGGGGGGGGGGGG..",
    ".GGGGGGGGGGGGG..",
    "..GGGGGGGGGGG...",
    "..GGGGGGGGGGG...",
    "...GGGGGGGGG....",
    "....GGGGGGG.....",
]


def render(rows, palette):
    flat = []
    for row in rows:
        for ch in row:
            flat.append(palette.get(ch, TRANSPARENT))
    return flat


fb_palette = {"R": R, "D": RD, "W": W, "K": K}
mac_palette = {"G": G, "D": GD, "L": L}


def main():
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    platforms = os.path.join(root, "platforms")

    fb_rows, fb_w = grid(freebsd)
    encode_png(os.path.join(platforms, "freebsd.png"),
               render(fb_rows, fb_palette), fb_w, len(fb_rows))
    print("wrote platforms/freebsd.png (%dx%d)" % (fb_w, len(fb_rows)))

    mac_rows, mac_w = grid(macos)
    encode_png(os.path.join(platforms, "macos.png"),
               render(mac_rows, mac_palette), mac_w, len(mac_rows))
    print("wrote platforms/macos.png (%dx%d)" % (mac_w, len(mac_rows)))


if __name__ == "__main__":
    main()
