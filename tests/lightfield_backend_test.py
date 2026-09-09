#!/usr/bin/env python3
"""Strict unscaled C/native-Go pixel comparison of the transpiled example."""

import argparse
import json
from pathlib import Path

from PIL import Image
from lightfield_reference_test import compare
from lightfield_motion_test import CAPTURE_STAGES


def read_capture(directory, name):
    path = directory / name
    with Image.open(path) as image:
        if image.size != (768, 1024):
            raise SystemExit(f"{path}: expected native 768x1024 capture")
        if image.convert("RGBA").getchannel("A").getextrema() != (255, 255):
            raise SystemExit(f"{path}: opaque example capture contains transparency")
        return image.convert("RGB")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--c-captures", type=Path, required=True)
    parser.add_argument("--go-captures", type=Path, required=True)
    args = parser.parse_args()
    regions = {
        "whole_frame": (0, 0, 768, 1024),
        "states": (16, 130, 750, 460),
        "sizes": (16, 476, 750, 658),
        "icons": (16, 684, 750, 850),
        "full_width": (16, 876, 750, 988),
    }
    report = {}
    motion = {}
    exact = True
    for theme in ("light", "dark"):
        c = read_capture(args.c_captures, f"{theme}.png")
        go = read_capture(args.go_captures, f"{theme}.png")
        report[theme] = {}
        for name, bounds in regions.items():
            result = compare(c, go, 0, bounds)
            report[theme][name] = result
            exact = exact and result["exact"]
        motion[theme] = {}
        for stage in CAPTURE_STAGES[1:]:
            c = read_capture(args.c_captures, f"{theme}{stage}.png")
            go = read_capture(args.go_captures, f"{theme}{stage}.png")
            motion[theme][stage] = {}
            for name, bounds in {
                "whole_frame": regions["whole_frame"],
                "control": (12, 734, 102, 800),
                "loading": (78, 410, 748, 450),
            }.items():
                result = compare(c, go, 0, bounds)
                motion[theme][stage][name] = result
                exact = exact and result["exact"]
    print(json.dumps({"exact": exact, "regions": report, "motion": motion}, indent=2))
    return 0 if exact else 1


if __name__ == "__main__":
    raise SystemExit(main())
