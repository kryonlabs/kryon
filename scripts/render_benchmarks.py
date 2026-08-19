#!/usr/bin/env python3
"""Merge text-input benchmark JSONL runs into docs/site/benchmarks.json.

Input lines are the JSON objects printed by tests/text_input_perf_test.c and
tests/text_input_precision_test.c (one per scenario). Output is consumed by
docs/site/benchmarks.html on the kryon labs site:

    python3 scripts/render_benchmarks.py out.json run1.jsonl run2.jsonl ...
"""

import json
import sys
import datetime


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out_path, inputs = sys.argv[1], sys.argv[2:]
    runs = []
    for path in inputs:
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if not line.startswith("{"):
                continue
            runs.append(json.loads(line))
    if not runs:
        sys.exit("no benchmark lines found")
    doc = {
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "what": "Text-input latency of the kryon retained core: one full "
                "declare/reconcile/layout/route/update frame per keystroke, "
                "so these are CPU-side microseconds - rendering is GPU-side "
                "and not included. Regenerate with: make perf-text-input-site",
        "runs": runs,
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print("wrote %s (%d runs)" % (out_path, len(runs)))


if __name__ == "__main__":
    main()
