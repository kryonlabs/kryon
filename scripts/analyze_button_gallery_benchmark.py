#!/usr/bin/env python3
"""Validate and summarize the Button gallery's isolated benchmark CSV."""

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


STYLES = (
    "Material",
    "Classic",
    "Lightfield-flat",
    "Lightfield-glow",
)
EMPHASES = ("Filled", "Soft", "Outline", "Ghost", "Link")
STATES = ("Normal", "Hover", "Pressed", "Focus", "Disabled", "Selected", "Loading")
TONES = ("Neutral", "Accent", "Danger", "Success", "Warning")


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[math.ceil(fraction * len(ordered)) - 1]


def report_spikes(grouped_frames, frame_records, calls, budget_us):
    """Flag frame-time spikes and bimodal distributions the median hides.

    A single-frame stall is one sample far above the rest of its cell; a
    bimodal cell splits into two stable modes, so its median is not a
    representative cost. Outliers are localized by tone and sample so a
    transition/cache effect can be told apart from random scheduling noise.
    """
    print(f"\nSpike report (frame time; budget {budget_us} us)")
    print("style, emphasis, frames, median us, max us, max/median, "
          "p95/median, >2x median, >3x median, over budget")
    for (style, emphasis), values in sorted(grouped_frames.items()):
        median_frame = statistics.median(values)
        if median_frame == 0:
            continue
        spikes_gt2x = sum(1 for value in values if value > 2 * median_frame)
        spikes_gt3x = sum(1 for value in values if value > 3 * median_frame)
        over_budget = sum(1 for value in values if value > budget_us)
        ratio_max = max(values) / median_frame
        ratio_p95 = percentile(values, 0.95) / median_frame
        flag = ""
        if ratio_p95 > 2:
            flag = "  <-- bimodal (p95/median > 2)"
        elif spikes_gt3x:
            flag = "  <-- single-frame spikes (>3x median)"
        print(f"{style}, {emphasis}, {len(values)}, {median_frame:.1f}, "
              f"{max(values):.1f}, {ratio_max:.2f}, {ratio_p95:.2f}, "
              f"{spikes_gt2x}, {spikes_gt3x}, {over_budget}{flag}")

    print("\nOutlier localization (tone, sample index within tone)")
    for (style, emphasis), values in sorted(grouped_frames.items()):
        median_frame = statistics.median(values)
        if median_frame == 0:
            continue
        records = frame_records.get((style, emphasis), [])
        outliers = [(tone, sample, frame_us) for tone, sample, frame_us in records
                    if frame_us > 3 * median_frame]
        if not outliers:
            continue
        by_tone = defaultdict(int)
        for tone, _sample, _us in outliers:
            by_tone[tone] += 1
        shown = ", ".join(f"{tone}@{sample}" for tone, sample, _ in outliers[:20])
        if len(outliers) > 20:
            shown += f" (+{len(outliers) - 20} more)"
        print(f"{style:<16} {emphasis:<8} {len(outliers):>3} outliers "
              f"(>3x median): {shown}; by tone: {dict(by_tone)}")

    print("\nWorst Button-call cells (max us)")
    print("style, emphasis, state, median us, max us")
    worst = sorted((item for item in calls.items() if item[1]),
                   key=lambda item: max(item[1]), reverse=True)[:10]
    for (style, emphasis, state), values in worst:
        print(f"{style}, {emphasis}, {state}, "
              f"{statistics.median(values):.2f}, {max(values):.2f}")


def print_histogram(grouped_frames):
    """Print a compact per-cell frame-time histogram (12 buckets, min..max)."""
    print("\nFrame-time histograms (matrix cells; 12 buckets min..max)")
    for (style, emphasis), values in sorted(grouped_frames.items()):
        if emphasis == "all" or not values:
            continue
        low, high = min(values), max(values)
        span = (high - low) or 1.0
        buckets = [0] * 12
        for value in values:
            index = min(int((value - low) / span * 12), 11)
            buckets[index] += 1
        peak = max(buckets)
        bar = "".join("=" * round(b / peak * 40) if b else "·" for b in buckets)
        counts = " ".join(f"{b:3d}" for b in buckets)
        print(f"{style:<16} {emphasis:<8} [{low:6.0f}..{high:6.0f}]us  {bar}")
        print(f"{'':<26} counts: {counts}")


def diff_baseline(grouped_frames, baseline_path):
    """Compare the current run against a prior summary CSV (regression check).

    Older summaries may lack the spike columns; those metrics are skipped
    rather than required.
    """
    baseline = {}
    with baseline_path.open(newline="") as source:
        for row in csv.DictReader(source):
            baseline[(row["style"], row["emphasis"])] = row
    have_spikes = all("frame_spikes_gt3x" in base for base in baseline.values())
    have_max = all("max_frame_us" in base for base in baseline.values())
    print(f"\nRegression diff vs {baseline_path}")
    header = "style, emphasis, median, p95"
    header += ", max" if have_max else ""
    header += ", spikes>3x" if have_spikes else ""
    print(header + "  (before -> after)")
    for (style, emphasis), values in sorted(grouped_frames.items()):
        if emphasis == "all":
            continue
        base = baseline.get((style, emphasis))
        if not base:
            continue
        median_before = float(base["median_frame_us"])
        p95_before = float(base["p95_frame_us"])
        median_after = statistics.median(values)
        p95_after = percentile(values, 0.95)

        def pair(after, before):
            change = 100 * (after - before) / before if before else 0.0
            return f"{before:7.0f}->{after:7.0f} ({change:+.1f}%)"

        line = (f"{style:<16} {emphasis:<8} "
                f"med {pair(median_after, median_before)}  "
                f"p95 {pair(p95_after, p95_before)}")
        if have_max:
            line += f"  max {pair(max(values), float(base['max_frame_us']))}"
        if have_spikes:
            spikes_after = sum(1 for v in values
                               if v > 3 * median_after)
            line += f"  spikes {base['frame_spikes_gt3x']}->{spikes_after}"
        print(line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path)
    parser.add_argument("summary", type=Path)
    parser.add_argument("--budget-us", type=int, default=16_667,
                        help="frame budget in microseconds (default 16667 = 60 FPS); "
                             "frames over this are counted as over budget")
    parser.add_argument("--histogram", action="store_true",
                        help="print a per-cell frame-time histogram")
    parser.add_argument("--baseline", type=Path, default=None,
                        help="a prior summary CSV to diff against (regression check)")
    args = parser.parse_args()

    cells = set()
    frames = {}
    frame_records = defaultdict(list)
    calls = defaultdict(list)
    count = 0
    with args.raw.open(newline="") as source:
        reader = csv.DictReader(source)
        for row in reader:
            count += 1
            page = int(row["page"])
            sample = int(row["sample"])
            emphasis = row["emphasis"]
            state = row["state"]
            style = row["style"]
            expected_style = STYLES[page // 5] if page < 20 else STYLES[page - 20]
            assert style == expected_style, (page, style)
            assert 0 <= sample < 100, (page, sample)
            assert row["glow"] == ("1" if style == "Lightfield-glow" else "0")
            assert row["mode"] == ("light" if style == "Classic" else "dark")
            call_us = float(row["button_call_us"])
            frame_us = float(row["frame_us"])
            assert math.isfinite(call_us) and call_us >= 0
            assert math.isfinite(frame_us) and frame_us > 0
            cell = (page, emphasis, state, sample)
            assert cell not in cells, cell
            cells.add(cell)
            if page < 20:
                assert row["tone"] == TONES[page % 5]
                assert emphasis in EMPHASES and state in STATES
                calls[(style, emphasis, state)].append(call_us)
            else:
                assert (row["tone"], emphasis, state, call_us) == (
                    "variants", "all", "all", 0.0
                )
            frame_key = (page, emphasis, sample)
            if frame_key in frames:
                assert frames[frame_key][2] == frame_us, frame_key
            else:
                frames[frame_key] = (style, emphasis, frame_us)
                frame_records[(style, emphasis)].append(
                    (row["tone"], sample, frame_us))

    assert count == 70_400, count
    assert len(frames) == 10_400, len(frames)
    for page in range(24):
        if page < 20:
            for emphasis in EMPHASES:
                for state in STATES:
                    for sample in range(100):
                        assert (page, emphasis, state, sample) in cells
        else:
            for sample in range(100):
                assert (page, "all", "all", sample) in cells

    grouped_frames = defaultdict(list)
    for style, emphasis, frame_us in frames.values():
        grouped_frames[(style, emphasis)].append(frame_us)

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    with args.summary.open("w", newline="") as destination:
        writer = csv.writer(destination)
        writer.writerow(("style", "emphasis", "frames", "median_frame_us",
                         "p95_frame_us", "median_button_call_us",
                         "p95_button_call_us", "p99_9_frame_us",
                         "max_frame_us", "frame_stddev_us",
                         "frame_spikes_gt2x", "frame_spikes_gt3x",
                         "frame_over_budget", "max_button_call_us",
                         "min_frame_us", "min_button_call_us"))
        print("style, emphasis, frames, median frame us, p95 frame us, median Button call us")
        for style in STYLES:
            for emphasis in (*EMPHASES, "all"):
                values = grouped_frames.get((style, emphasis))
                if not values:
                    continue
                calls_for_emphasis = [value for state in STATES
                                      for value in calls[(style, emphasis, state)]]
                median_frame = statistics.median(values)
                row = (style, emphasis, len(values),
                       round(median_frame, 3),
                       round(percentile(values, 0.95), 3),
                       round(statistics.median(calls_for_emphasis), 3)
                       if calls_for_emphasis else "",
                       round(percentile(calls_for_emphasis, 0.95), 3)
                       if calls_for_emphasis else "",
                       round(percentile(values, 0.999), 3),
                       round(max(values), 3),
                       round(statistics.pstdev(values), 3),
                       sum(1 for value in values if value > 2 * median_frame),
                       sum(1 for value in values if value > 3 * median_frame),
                       sum(1 for value in values if value > args.budget_us),
                       round(max(calls_for_emphasis), 3)
                       if calls_for_emphasis else "",
                       round(min(values), 3),
                       round(min(calls_for_emphasis), 3)
                       if calls_for_emphasis else "")
                writer.writerow(row)
                print(", ".join(map(str, row[:6])))
    print(f"validated {count} cells in {len(frames)} measured frames")

    report_spikes(grouped_frames, frame_records, calls, args.budget_us)
    if args.histogram:
        print_histogram(grouped_frames)
    if args.baseline:
        diff_baseline(grouped_frames, args.baseline)


if __name__ == "__main__":
    main()
