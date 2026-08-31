#!/usr/bin/env python3
"""Generate pixel-art RGBA PNGs for selected Kryon icons.

Kryon's checked-in icons/ tree is the source of truth for embedded UI icons
and website assets. This script hand-crafts deterministic pixel-art icons
using raw RGBA pixels and encodes them as PNGs via the stdlib.
"""
import struct
import zlib
import math

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

# ---- macOS: graphite apple silhouette with bite + leaf ----
G = (65, 65, 69, 255)
GD = (42, 42, 46, 255)
L = (104, 164, 82, 255)

macos = [
    "................",
    ".......LL.......",
    "......LLL.......",
    "................",
    "....GGG..GG.....",
    "...GGGGGGGGG....",
    "..GGGGGGGG..G...",
    "..GGGGGGG.......",
    "..GGGGGGGG......",
    "..GGGGGGGGG.....",
    "...GGGGGGGG.....",
    "...GGGGGGG......",
    "....GGGGGG......",
    ".....GGGG.......",
    "................",
    "................",
]


def render(rows, palette):
    flat = []
    for row in rows:
        for ch in row:
            flat.append(palette.get(ch, TRANSPARENT))
    return flat


def canvas(width, height, color=TRANSPARENT):
    return [color] * (width * height)


def put(pixels, width, height, x, y, color):
    if 0 <= x < width and 0 <= y < height:
        pixels[y * width + x] = color


def rect(pixels, width, height, x0, y0, x1, y1, color):
    for y in range(max(0, y0), min(height, y1)):
        for x in range(max(0, x0), min(width, x1)):
            pixels[y * width + x] = color


def circle(pixels, width, height, cx, cy, radius, color):
    rr = radius * radius
    for y in range(cy - radius, cy + radius + 1):
        for x in range(cx - radius, cx + radius + 1):
            dx = x - cx
            dy = y - cy
            if dx * dx + dy * dy <= rr:
                put(pixels, width, height, x, y, color)


def ring(pixels, width, height, cx, cy, outer, inner, color):
    oo = outer * outer
    ii = inner * inner
    for y in range(cy - outer, cy + outer + 1):
        for x in range(cx - outer, cx + outer + 1):
            dx = x - cx
            dy = y - cy
            d = dx * dx + dy * dy
            if ii <= d <= oo:
                put(pixels, width, height, x, y, color)


def line(pixels, width, height, x0, y0, x1, y1, color, thickness=1):
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    half = thickness // 2
    for i in range(steps + 1):
        t = i / steps
        x = int(round(x0 + (x1 - x0) * t))
        y = int(round(y0 + (y1 - y0) * t))
        rect(pixels, width, height, x - half, y - half,
             x - half + thickness, y - half + thickness, color)


def polygon(pixels, width, height, points, color):
    min_y = max(0, min(y for _, y in points))
    max_y = min(height - 1, max(y for _, y in points))
    n = len(points)
    for y in range(min_y, max_y + 1):
        xs = []
        for i in range(n):
            x1, y1 = points[i]
            x2, y2 = points[(i + 1) % n]
            if y1 == y2:
                continue
            if (y >= min(y1, y2)) and (y < max(y1, y2)):
                xs.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
        xs.sort()
        for i in range(0, len(xs), 2):
            if i + 1 >= len(xs):
                break
            x0 = max(0, int(math.ceil(xs[i])))
            x1 = min(width - 1, int(math.floor(xs[i + 1])))
            for x in range(x0, x1 + 1):
                put(pixels, width, height, x, y, color)


def rounded_rect(pixels, width, height, x0, y0, x1, y1, radius, color):
    rect(pixels, width, height, x0 + radius, y0, x1 - radius, y1, color)
    rect(pixels, width, height, x0, y0 + radius, x1, y1 - radius, color)
    circle(pixels, width, height, x0 + radius, y0 + radius, radius, color)
    circle(pixels, width, height, x1 - radius - 1, y0 + radius, radius, color)
    circle(pixels, width, height, x0 + radius, y1 - radius - 1, radius, color)
    circle(pixels, width, height, x1 - radius - 1, y1 - radius - 1, radius, color)


def make_debian():
    rows = [
        "................",
        "....RRRRRR......",
        "..RRRRRRRRR.....",
        ".RRRR...RRRR....",
        ".RR......RRR....",
        "RRR......RRR....",
        "RRR.....RRR.....",
        ".RRR...RRR......",
        "..RRRRRRD.......",
        "...RRRDD........",
        ".....DDRR.......",
        ".....DRR........",
        "....RRR.........",
        "..RRR...........",
        ".RR.............",
        "................",
    ]
    palette = {
        "R": (190, 0, 58, 255),
        "D": (130, 0, 44, 255),
    }

    grid_rows, width = grid(rows)
    return render(grid_rows, palette), width, len(grid_rows)


def make_c():
    rows = [
        "................",
        "................",
        "......GGGGG.....",
        "....GGGGGGGG....",
        "...GGG....GG....",
        "...GG...........",
        "..GGG...........",
        "..GGG...........",
        "..GGG...........",
        "..GGG...........",
        "...GG...........",
        "...GGG....GG....",
        "....GGGGGGGG....",
        "......GGGGG.....",
        "................",
        "................",
    ]
    palette = {
        "G": (108, 222, 115, 255),
    }
    rows, width = grid(rows)
    return render(rows, palette), width, len(rows)


def make_appimage():
    w = h = 64
    px = canvas(w, h)
    blue = (96, 154, 183, 255)
    blue_dark = (42, 80, 98, 255)
    blue_mid = (74, 130, 160, 255)
    blue_light = (171, 205, 222, 255)
    white = (245, 247, 248, 255)
    white_dim = (226, 231, 234, 255)

    rounded_rect(px, w, h, 5, 8, 59, 58, 5, blue_dark)
    rounded_rect(px, w, h, 7, 10, 57, 56, 4, blue)
    rect(px, w, h, 9, 11, 55, 14, blue_light)
    rect(px, w, h, 9, 50, 55, 55, blue_mid)

    # Arrow with a hard pixel silhouette.
    rect(px, w, h, 28, 15, 36, 32, white)
    polygon(px, w, h, [(20, 30), (44, 30), (32, 43)], white)
    rect(px, w, h, 30, 15, 34, 31, white_dim)

    # Gear: cardinal/diagonal teeth plus ring.
    rect(px, w, h, 29, 39, 35, 54, white)
    rect(px, w, h, 29, 39, 35, 54, white)
    rect(px, w, h, 18, 45, 46, 51, white)
    polygon(px, w, h, [(21, 36), (25, 32), (31, 38), (27, 42)], white)
    polygon(px, w, h, [(43, 36), (39, 32), (33, 38), (37, 42)], white)
    polygon(px, w, h, [(21, 60), (27, 54), (31, 58), (25, 63)], white)
    polygon(px, w, h, [(43, 60), (37, 54), (33, 58), (39, 63)], white)
    circle(px, w, h, 32, 49, 12, white)
    ring(px, w, h, 32, 49, 12, 7, white_dim)
    circle(px, w, h, 32, 49, 6, blue_mid)
    circle(px, w, h, 32, 49, 2, blue)
    return px, w, h


def make_snap():
    w = h = 64
    px = canvas(w, h)
    green = (128, 188, 160, 255)
    green_dark = (91, 154, 129, 255)
    orange = (244, 90, 54, 255)

    # Pixel-origami snapcraft-style bird: three green folds and an orange beak.
    polygon(px, w, h, [(1, 8), (36, 22), (36, 43), (26, 35)], green)
    polygon(px, w, h, [(38, 23), (51, 31), (38, 43)], green)
    polygon(px, w, h, [(27, 36), (36, 44), (14, 62)], green_dark)
    polygon(px, w, h, [(39, 22), (59, 22), (63, 34), (52, 31)], orange)

    # Small transparent cuts keep the folds readable at icon sizes.
    polygon(px, w, h, [(36, 22), (38, 23), (38, 44), (36, 43)], TRANSPARENT)
    line(px, w, h, 27, 35, 36, 43, TRANSPARENT, 2)
    line(px, w, h, 39, 22, 52, 31, TRANSPARENT, 2)
    return px, w, h


fb_palette = {"R": R, "D": RD, "W": W, "K": K}
mac_palette = {"G": G, "D": GD, "L": L}
eye_palette = {"W": (245, 245, 245, 255)}

eye = [
    "................",
    "................",
    "................",
    ".....WWWWWW.....",
    "...WW......WW...",
    "..WW........WW..",
    ".WW....WW....WW.",
    ".WW...WWWW...WW.",
    ".WW...WWWW...WW.",
    ".WW....WW....WW.",
    "..WW........WW..",
    "...WW......WW...",
    ".....WWWWWW.....",
    "................",
    "................",
    "................",
]

eye_off = [
    "................",
    ".............WW.",
    "............WW..",
    ".....WWWWW.WW...",
    "...WW.....WWWW..",
    "..WW.....WW..WW.",
    ".WW....WW.....WW",
    ".WW...WWWW...WW.",
    ".WW..WW.WW...WW.",
    ".WW.WW..WW...WW.",
    "..WW........WW..",
    ".WWWW......WW...",
    "WW...WWWWWW.....",
    "W...............",
    "................",
    "................",
]

workbook_palette = {"W": (245, 247, 248, 255)}

workbook_clear_formatting = [
    "................",
    "..WW........WW..",
    "...WW......WW...",
    "....WW....WW....",
    ".....WW..WW.....",
    "......WWWW......",
    ".......WW.......",
    "......WWWW......",
    ".....WW..WW.....",
    "....WW....WW....",
    "...WW......WW...",
    "..WW........WW..",
    "................",
    "................",
    "................",
    "................",
]

workbook_fill_color = [
    "................",
    ".....WWWW.......",
    "....WWWWWW......",
    "...WWW..WWW.....",
    "..WWW....WWW....",
    ".WWW......WWW...",
    "..WWW....WWW....",
    "...WWW..WWW.....",
    "....WWWWWW......",
    ".....WWWW.......",
    "................",
    "..WWWWWWWWWWWW..",
    "..WWWWWWWWWWWW..",
    "..WWWWWWWWWWWW..",
    "................",
    "................",
]

workbook_text_color = [
    "................",
    "......WWWW......",
    ".....WWWWWW.....",
    "....WWW..WWW....",
    "....WW....WW....",
    "...WWWWWWWWWW...",
    "...WWWWWWWWWW...",
    "..WWW......WWW..",
    "..WW........WW..",
    ".WWWW......WWWW.",
    "................",
    "..WWWWWWWWWWWW..",
    "..WWWWWWWWWWWW..",
    "..WWWWWWWWWWWW..",
    "................",
    "................",
]


def main():
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    icons = os.path.join(root, "icons")
    platforms = os.path.join(root, "icons", "platforms")
    workbook = os.path.join(root, "icons", "workbook")
    os.makedirs(platforms, exist_ok=True)
    os.makedirs(workbook, exist_ok=True)

    c_px, c_w, c_h = make_c()
    encode_png(os.path.join(icons, "c.png"), c_px, c_w, c_h)
    print("wrote icons/c.png (%dx%d)" % (c_w, c_h))

    eye_rows, eye_w = grid(eye)
    encode_png(os.path.join(icons, "eye.png"),
               render(eye_rows, eye_palette), eye_w, len(eye_rows))
    print("wrote icons/eye.png (%dx%d)" % (eye_w, len(eye_rows)))

    eye_off_rows, eye_off_w = grid(eye_off)
    encode_png(os.path.join(icons, "eye_off.png"),
               render(eye_off_rows, eye_palette), eye_off_w, len(eye_off_rows))
    print("wrote icons/eye_off.png (%dx%d)" %
          (eye_off_w, len(eye_off_rows)))

    debian_px, debian_w, debian_h = make_debian()
    encode_png(os.path.join(platforms, "debian.png"),
               debian_px, debian_w, debian_h)
    print("wrote icons/platforms/debian.png (%dx%d)" % (debian_w, debian_h))

    appimage_px, appimage_w, appimage_h = make_appimage()
    encode_png(os.path.join(platforms, "appimage.png"),
               appimage_px, appimage_w, appimage_h)
    print("wrote icons/platforms/appimage.png (%dx%d)" %
          (appimage_w, appimage_h))

    snap_px, snap_w, snap_h = make_snap()
    encode_png(os.path.join(platforms, "snap.png"), snap_px, snap_w, snap_h)
    print("wrote icons/platforms/snap.png (%dx%d)" % (snap_w, snap_h))

    fb_rows, fb_w = grid(freebsd)
    encode_png(os.path.join(platforms, "freebsd.png"),
               render(fb_rows, fb_palette), fb_w, len(fb_rows))
    print("wrote icons/platforms/freebsd.png (%dx%d)" % (fb_w, len(fb_rows)))

    mac_rows, mac_w = grid(macos)
    encode_png(os.path.join(platforms, "macos.png"),
               render(mac_rows, mac_palette), mac_w, len(mac_rows))
    print("wrote icons/platforms/macos.png (%dx%d)" % (mac_w, len(mac_rows)))

    clear_rows, clear_w = grid(workbook_clear_formatting)
    encode_png(os.path.join(workbook, "clear_formatting.png"),
               render(clear_rows, workbook_palette), clear_w, len(clear_rows))
    print("wrote icons/workbook/clear_formatting.png (%dx%d)" %
          (clear_w, len(clear_rows)))

    fill_rows, fill_w = grid(workbook_fill_color)
    encode_png(os.path.join(workbook, "fill_color.png"),
               render(fill_rows, workbook_palette), fill_w, len(fill_rows))
    print("wrote icons/workbook/fill_color.png (%dx%d)" %
          (fill_w, len(fill_rows)))

    text_rows, text_w = grid(workbook_text_color)
    encode_png(os.path.join(workbook, "text_color.png"),
               render(text_rows, workbook_palette), text_w, len(text_rows))
    print("wrote icons/workbook/text_color.png (%dx%d)" %
          (text_w, len(text_rows)))


if __name__ == "__main__":
    main()
