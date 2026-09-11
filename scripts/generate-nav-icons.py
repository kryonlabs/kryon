#!/usr/bin/env python3
"""Generate Kryon navigation SVG sources and raster atlas entries."""

from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "icons"
SOURCE_DIR = ICON_DIR / "sources" / "nav"
MANIFEST_PATH = ICON_DIR / "ui.json"
ATLAS_PATH = ICON_DIR / "ui.png"
SIZE = 64
SCALE = 4


SVG_SOURCES = {
    "nav_list": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke="white" stroke-width="4.4" stroke-linecap="round" stroke-linejoin="round">
    <rect x="18" y="14" width="28" height="36" rx="5"/>
    <path d="M25 25h14M25 32h14M25 39h10"/>
  </g>
</svg>
""",
    "nav_habits": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke="white" stroke-width="4.6" stroke-linecap="round" stroke-linejoin="round">
    <path d="M32 48C31 39 32 29 32 18"/>
    <path d="M31 33C22 33 17 27 16 20C24 20 30 24 31 33Z"/>
    <path d="M33 29C43 29 48 23 49 16C40 16 34 20 33 29Z"/>
  </g>
</svg>
""",
    "nav_practice": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke="white" stroke-width="4.4" stroke-linecap="round" stroke-linejoin="round">
    <path d="M32 49C24 39 25 25 32 13C39 25 40 39 32 49Z"/>
    <path d="M30 48C18 45 12 34 14 22C25 25 31 35 30 48Z"/>
    <path d="M34 48C46 45 52 34 50 22C39 25 33 35 34 48Z"/>
    <path d="M20 48h24"/>
  </g>
</svg>
""",
    "nav_settings": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke="white" stroke-width="4.2" stroke-linecap="round" stroke-linejoin="round">
    <path d="M32 13l4 5 7-1 2 7 6 4-4 6 2 7-7 3-3 7-7-2-7 2-3-7-7-3 2-7-4-6 6-4 2-7 7 1z"/>
    <circle cx="32" cy="32" r="7.5"/>
  </g>
</svg>
""",
}


def point(x: float, y: float) -> tuple[int, int]:
    return (round(x * SCALE), round(y * SCALE))


def cubic(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    steps: int = 20,
) -> list[tuple[float, float]]:
    pts = []
    for i in range(steps + 1):
        t = i / steps
        mt = 1.0 - t
        x = (
            mt * mt * mt * p0[0]
            + 3 * mt * mt * t * p1[0]
            + 3 * mt * t * t * p2[0]
            + t * t * t * p3[0]
        )
        y = (
            mt * mt * mt * p0[1]
            + 3 * mt * mt * t * p1[1]
            + 3 * mt * t * t * p2[1]
            + t * t * t * p3[1]
        )
        pts.append((x, y))
    return pts


def draw_path(draw: ImageDraw.ImageDraw, pts: list[tuple[float, float]], width: float) -> None:
    draw.line([point(x, y) for x, y in pts], fill=(255, 255, 255, 255), width=round(width * SCALE), joint="curve")


def draw_list(draw: ImageDraw.ImageDraw) -> None:
    w = round(4.4 * SCALE)
    draw.rounded_rectangle([18 * SCALE, 14 * SCALE, 46 * SCALE, 50 * SCALE], radius=5 * SCALE,
                           outline=(255, 255, 255, 255), width=w)
    for y, x2 in ((25, 39), (32, 39), (39, 35)):
        draw.line([point(25, y), point(x2, y)], fill=(255, 255, 255, 255), width=w)


def draw_habits(draw: ImageDraw.ImageDraw) -> None:
    w = 4.6
    draw_path(draw, cubic((32, 48), (31, 39), (32, 29), (32, 18)), w)
    left = cubic((31, 33), (23, 33), (18, 28), (16, 20)) + cubic((16, 20), (24, 20), (30, 24), (31, 33))
    right = cubic((33, 29), (42, 29), (47, 23), (49, 16)) + cubic((49, 16), (40, 16), (34, 20), (33, 29))
    draw_path(draw, left, w)
    draw_path(draw, right, w)


def draw_practice(draw: ImageDraw.ImageDraw) -> None:
    w = 4.4
    center = cubic((32, 49), (24, 39), (25, 25), (32, 13)) + cubic((32, 13), (39, 25), (40, 39), (32, 49))
    left = cubic((30, 48), (18, 45), (12, 34), (14, 22)) + cubic((14, 22), (25, 25), (31, 35), (30, 48))
    right = cubic((34, 48), (46, 45), (52, 34), (50, 22)) + cubic((50, 22), (39, 25), (33, 35), (34, 48))
    draw_path(draw, center, w)
    draw_path(draw, left, w)
    draw_path(draw, right, w)
    draw.line([point(20, 48), point(44, 48)], fill=(255, 255, 255, 255), width=round(w * SCALE))


def draw_settings(draw: ImageDraw.ImageDraw) -> None:
    w = round(4.2 * SCALE)
    pts = []
    for i in range(16):
        angle = -math.pi / 2 + i * math.pi / 8
        radius = 19 if i % 2 == 0 else 14.5
        pts.append(point(32 + math.cos(angle) * radius, 32 + math.sin(angle) * radius))
    pts.append(pts[0])
    draw.line(pts, fill=(255, 255, 255, 255), width=w, joint="curve")
    draw.ellipse([24.5 * SCALE, 24.5 * SCALE, 39.5 * SCALE, 39.5 * SCALE], outline=(255, 255, 255, 255), width=w)


DRAWERS = {
    "nav_list": draw_list,
    "nav_habits": draw_habits,
    "nav_practice": draw_practice,
    "nav_settings": draw_settings,
}


def render(name: str) -> Image.Image:
    large = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE), (0, 0, 0, 0))
    DRAWERS[name](ImageDraw.Draw(large))
    return large.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def main() -> None:
    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    for name, svg in SVG_SOURCES.items():
        (SOURCE_DIR / f"{name}.svg").write_text(svg)

    manifest = json.loads(MANIFEST_PATH.read_text())
    atlas = Image.open(ATLAS_PATH).convert("RGBA")
    icons = [icon for icon in manifest["icons"] if icon["name"] not in DRAWERS]

    used = {(icon["x"] // SIZE, icon["y"] // SIZE) for icon in icons}
    width_cells = manifest["columns"]

    for name in DRAWERS:
        slot = None
        rows = max(manifest["rows"], math.ceil(atlas.height / SIZE))
        for row in range(rows):
            for col in range(width_cells):
                if (col, row) not in used:
                    slot = (col, row)
                    break
            if slot is not None:
                break
        if slot is None:
            slot = (0, rows)
            new_atlas = Image.new("RGBA", (atlas.width, atlas.height + SIZE), (0, 0, 0, 0))
            new_atlas.alpha_composite(atlas, (0, 0))
            atlas = new_atlas
            manifest["rows"] = rows + 1
            manifest["height"] = atlas.height

        col, row = slot
        used.add(slot)
        atlas.alpha_composite(render(name), (col * SIZE, row * SIZE))
        icons.append({
            "index": 0,
            "name": name,
            "x": col * SIZE,
            "y": row * SIZE,
            "width": SIZE,
            "height": SIZE,
            "upstream": f"generated/nav/{name}.svg",
        })

    icons.sort(key=lambda icon: (icon["y"], icon["x"], icon["name"]))
    for index, icon in enumerate(icons):
        icon["index"] = index

    manifest["icon_count"] = len(icons)
    manifest["rows"] = max(manifest["rows"], math.ceil(atlas.height / SIZE))
    manifest["height"] = atlas.height
    manifest["icons"] = icons

    atlas.save(ATLAS_PATH)
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
