#!/usr/bin/env python3
"""Require offset/composed panels to preserve native pixels without rescaling."""

import argparse
from pathlib import Path

from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", required=True, type=Path)
    parser.add_argument("--isolated-offset", action="store_true")
    args = parser.parse_args()
    with Image.open(args.captures / "split.png") as source:
        assert source.size == (1536, 1024), source.size
        split = source.convert("RGBA")
    # Exclude the deliberately different header and theme-selection controls.
    # Keep the panel, labels, material glow, and footer at native resolution.
    for theme, offset in (("dark", 0), ("light", 768)):
        with Image.open(args.captures / f"{theme}.png") as source:
            assert source.size == (768, 1024), source.size
            expected = source.convert("RGBA").crop((0, 74, 768, 1024))
        actual = split.crop((offset, 74, offset + 768, 1024))
        difference = ImageChops.difference(expected, actual)
        # RGB and alpha must both match; RGBA getbbox alone can hide RGB errors.
        assert all(channel.getbbox() is None for channel in difference.split()), theme
    if args.isolated_offset:
        with Image.open(args.captures / "light-offset.png") as source:
            assert source.size == (1536, 1024), source.size
            isolated = source.convert("RGBA").crop((768, 74, 1536, 1024))
        difference = ImageChops.difference(isolated, split.crop((768, 74, 1536, 1024)))
        assert all(channel.getbbox() is None for channel in difference.split()), "isolated offset"
    print("Lightfield panels preserve exact pixels under translation and composition")


if __name__ == "__main__":
    main()
