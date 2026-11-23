#!/usr/bin/env python3
"""Validate and summarize a completed native Moto VRM benchmark log."""
import json
import math
from pathlib import Path
import re
import statistics
import sys


def summarize(path):
    header = None
    samples = {0: {}, 1: {}}
    pattern = re.compile(
        r"sample mode=(\d+) frame=(\d+) spring_ns=(\d+) skin_ns=(\d+) "
        r"render_ns=(\d+) total_ns=(\d+) covered=(\d+)$")
    with path.open() as source:
        for line in source:
            line = line.strip()
            if line.startswith("model="):
                assert header is None, "Duplicate benchmark header"
                header = line
            elif line.startswith("sample "):
                match = pattern.fullmatch(line)
                assert match, "Malformed benchmark sample"
                mode, frame, spring, skin, render, total, covered = map(int, match.groups())
                assert mode in samples and frame not in samples[mode], "Duplicate or unknown sample"
                assert covered > 0 and total >= spring + skin + render, "Invalid rendered frame"
                samples[mode][frame] = dict(spring=spring, skin=skin, render=render, total=total)
            elif line:
                raise AssertionError("Unexpected benchmark output: " + line[:200])
    assert header and "spring_joints=0 " not in header, "Missing avatar or spring simulation"
    assert "renderer=" in header and header.split("renderer=", 1)[1], "Missing GPU renderer"
    result = {"header": header, "resolution": [600, 800], "warmup_frames": 12,
              "identity": path.with_suffix(path.suffix + ".identity").read_text().splitlines(),
              "modes": {}}
    for mode, frames in samples.items():
        assert set(frames) == set(range(12, 132)), "Incomplete measured frame sequence"
        metrics = {}
        for metric in ("spring", "skin", "render", "total"):
            values = sorted(frame[metric] / 1_000_000 for frame in frames.values())
            metrics[metric + "_ms"] = {
                "median": round(statistics.median(values), 4),
                "p95": round(values[math.ceil(len(values) * .95) - 1], 4),
                "max": round(max(values), 4),
            }
        result["modes"]["linear" if mode == 0 else "dual_quaternion"] = metrics
    return result


if __name__ == "__main__":
    assert len(sys.argv) == 2, "Pass one completed benchmark log"
    print(json.dumps(summarize(Path(sys.argv[1])), indent=2))
