#!/usr/bin/env python3
"""Read-only, unscaled comparisons against the user-approved generated baseline.

This is intentionally not a bless/update-golden test: current output must not
silently replace the user's reference. Use --reference for an optional comparison
with the original design proposal. RGB error is reported on the 0..255 channel
scale. An exact match requires zero error.
"""

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def compare(reference, actual, offset, box):
    x0, y0, x1, y1 = box
    expected = reference.crop((x0 + offset, y0, x1 + offset, y1))
    observed = actual.crop(box)
    difference = ImageChops.difference(expected, observed)
    stats = ImageStat.Stat(difference)
    return {
        "rmse": math.sqrt(sum(channel * channel for channel in stats.rms) / 3),
        "maximum_channel_error": max(high for low, high in difference.getextrema()),
        "exact": difference.getbbox() is None,
    }


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    sources = parser.add_mutually_exclusive_group()
    sources.add_argument("--reference", type=Path,
                         help="Optional split design reference instead of the approved baseline")
    sources.add_argument("--baseline", type=Path,
                         default=root / "design/lightfield-baseline",
                         help="Approved native dark, light, and split captures")
    parser.add_argument("--captures", type=Path, default=root / "build/linux-x86_64/lightfield")
    args = parser.parse_args()
    reference_path = args.reference or args.baseline / "split.png"
    reference = Image.open(reference_path).convert("RGB")
    if reference.size != (1536, 1024):
        raise SystemExit("Expected a 1536x1024 split reference; no rescaling is permitted")
    regions = {
        "whole_frame": (0, 0, 768, 1024),
        "panel": (0, 74, 768, 994),
        "panel_left_edge": (0, 90, 12, 978),
        "panel_right_edge": (754, 90, 768, 978),
        "states": (16, 130, 750, 460),
        "sizes": (16, 476, 750, 658),
        "icons": (16, 684, 750, 850),
        "full_width": (16, 876, 750, 988),
        "full_width_primary": (16, 897, 750, 941),
        "full_width_secondary": (16, 941, 750, 988),
        # Separate typography from the surrounding material. A lower panel
        # error must not obscure headings with the wrong face or tracking.
        "theme_heading": (16, 80, 355, 106),
        "theme_caption": (16, 108, 355, 126),
        "variant_headings": (78, 130, 750, 158),
        "warning_heading": (585, 130, 669, 158),
        "state_labels": (16, 160, 78, 449),
        "size_labels": (16, 508, 78, 645),
        "icon_labels": (16, 718, 750, 742),
        "sizes_heading": (16, 476, 200, 503),
        "icons_heading": (16, 678, 350, 709),
        "full_width_heading": (16, 870, 440, 900),
        "footer": (16, 999, 750, 1024),
    }
    # Include the immediate glow around each row, not only the button faces.
    # These diagnostics supplement the complete panel comparison above.
    columns = (
        ("primary", 78, 163), ("secondary", 163, 247),
        ("outline", 247, 332), ("ghost", 332, 416),
        ("danger", 416, 500), ("success", 500, 585),
        ("warning", 585, 669), ("link", 669, 747),
    )
    icon_columns = (
        ("text", 16, 100), ("leading", 100, 194),
        ("trailing", 194, 289), ("only", 289, 350),
        ("pill", 350, 470), ("square", 470, 530),
        ("split", 530, 646), ("dropdown", 646, 750),
    )
    for row, top, bottom in (("primary", 742, 794), ("secondary", 794, 850)):
        for variant, left, right in icon_columns:
            regions[f"icon_{row}_{variant}"] = (left, top, right, bottom)
    # Label crops complement, rather than replace, the complete controls.
    # These fixed reference coordinates expose typography regressions that
    # a better-aligned face or brighter rim could otherwise conceal.
    icon_labels = (
        ("text", 30, 90), ("leading", 138, 181),
        ("trailing", 209, 246), ("pill", 377, 441),
        ("split", 550, 590), ("dropdown", 667, 714),
    )
    for row, top in (("primary", 758), ("secondary", 812)):
        for variant, left, right in icon_labels:
            regions[f"icon_{row}_{variant}_label"] = (left, top, right, top + 19)
    # Size-dependent lighting needs its own diagnostics: a stronger large
    # reflection must not hide a regression in small or medium controls.
    for size, top, bottom in (("small", 507, 544), ("medium", 544, 594), ("large", 594, 658)):
        regions[f"size_{size}"] = (78, top, 747, bottom)
        for variant, left, right in columns:
            regions[f"size_{size}_{variant}"] = (left, top, right, bottom)
            # Isolate the lower edge and its immediate contact light. Keep
            # this fixed in reference coordinates: do not align the current
            # render to hide a misplaced bevel or alter the full-size checks.
            regions[f"size_{size}_{variant}_rim"] = (
                left, bottom - 16, right, bottom - 6)
    for state, top, height in (
        ("normal", 160, 34), ("hover", 206, 36), ("pressed", 256, 36),
        ("focus", 306, 38), ("disabled", 360, 38), ("loading", 410, 40),
    ):
        regions[f"state_{state}"] = (78, top - 3, 747, top + height + 5)
        # Keep semantic variants separate: improving a filled face must not
        # conceal a regression in an outline or borderless material.
        for variant, left, right in columns:
            regions[f"state_{state}_{variant}"] = (left, top - 3, right, top + height + 5)
            if state != "loading":
                # Keep the central label area separate from the rim and outer
                # light. This is a native-pixel crop, not a font-only mask or
                # an alignment/rescaling operation that could hide an error.
                regions[f"state_{state}_{variant}_label"] = (
                    left + 20, top + 6, right - 20, top + height - 6)
    report = {}
    exact = True
    for theme, offset in (("dark", 0), ("light", 768)):
        expected = reference
        if not args.reference:
            expected = Image.open(args.baseline / f"{theme}.png").convert("RGB")
            if expected.size != (768, 1024):
                raise SystemExit(f"{theme}: expected native 768x1024 baseline")
            offset = 0
        actual = Image.open(args.captures / f"{theme}.png")
        if actual.convert("RGBA").getchannel("A").getextrema() != (255, 255):
            raise SystemExit(f"{theme}: opaque example capture contains transparent pixels")
        actual = actual.convert("RGB")
        if actual.size != (768, 1024):
            raise SystemExit(f"{theme}: expected a native 768x1024 capture, got {actual.size}")
        report[theme] = {}
        for name, box in regions.items():
            result = compare(expected, actual, offset, box)
            report[theme][name] = result
            exact = exact and result["exact"]
    split = Image.open(args.captures / "split.png")
    if split.size != reference.size:
        raise SystemExit(f"split: expected native {reference.size}, got {split.size}")
    if split.convert("RGBA").getchannel("A").getextrema() != (255, 255):
        raise SystemExit("split: opaque example capture contains transparent pixels")
    split = split.convert("RGB")
    split_report = {
        "canvas": compare(reference, split, 0, (0, 0, 1536, 1024)),
        "header": compare(reference, split, 0, (0, 0, 1536, 74)),
        "header_brand": compare(reference, split, 0, (20, 8, 250, 60)),
        "header_title": compare(reference, split, 0, (450, 8, 1090, 39)),
        "header_subtitle": compare(reference, split, 0, (550, 40, 990, 63)),
    }
    exact = exact and all(result["exact"] for result in split_report.values())
    print(json.dumps({"reference": str(args.reference or args.baseline), "exact": exact,
                      "regions": report, "split": split_report}, indent=2))
    return 0 if exact else 1


if __name__ == "__main__":
    raise SystemExit(main())
