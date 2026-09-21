#!/usr/bin/env python3
"""Combine the Button gallery paint benchmark and score CSV into per-theme
CPU and RAM scores.

CPU score = median whole-frame paint time (all seven states) plus the
`ReadActivation` input-poll cost, which is the shared hit-test every
hover/press/click/focus event pays. RAM score = peak RSS delta for that theme
against a no-buttons baseline, measured per process.
"""

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path


STYLES = ("Material", "Classic", "Lightfield-flat", "Lightfield-glow")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paint_csv", type=Path,
                        help="button-gallery-benchmark-isolated.csv")
    parser.add_argument("score_csv", type=Path,
                        help="button-gallery-score.csv")
    args = parser.parse_args()

    frames = defaultdict(list)
    with args.paint_csv.open(newline="") as source:
        for row in csv.DictReader(source):
            if int(row["page"]) < 20:
                frames[row["style"]].append(float(row["frame_us"]))

    input_us = {}
    peak_rss = {}
    with args.score_csv.open(newline="") as source:
        for row in csv.DictReader(source):
            input_us[row["theme"]] = float(row["input_us"])
            peak_rss[row["theme"]] = int(row["peak_rss_kib"])

    baseline_rss = peak_rss.get("baseline", 0)
    input_values = list(input_us.values())
    global_input = statistics.median(input_values) if input_values else 0.0

    print(f"{'theme':<18}{'paint_med_us':>13}{'input_us':>10}"
          f"{'cpu_us':>10}{'rss_kib':>10}{'rss_delta_kib':>15}")
    for style in STYLES:
        paint = statistics.median(frames[style])
        cpu = paint + input_us.get(style, global_input)
        rss = peak_rss.get(style, 0)
        delta = rss - baseline_rss
        print(f"{style:<18}{paint:>13.1f}{input_us.get(style, global_input):>10.2f}"
              f"{cpu:>10.1f}{rss:>10}{delta:>15}")

    print()
    print("CPU score = median whole-frame paint (us) + ReadActivation input "
          "cost (us).")
    print("RAM score = peak RSS delta vs baseline (KiB).")
    print(f"input-poll cost is theme-independent; median across runs = "
          f"{global_input:.2f} us")


if __name__ == "__main__":
    main()
