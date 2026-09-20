#!/usr/bin/env python3
"""Verify the style bridge ledger covers current ratchet allowlists."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEDGER = ROOT / "plan" / "STYLE_BRIDGE_LEDGER.md"

CHROME_GETTERS = re.compile(r"\bGetTheme(Button|ButtonHover|Surface|Background|Text|Border|Selection|Circle|Icon|Link)\s*\(")
GO_BASE_PAT = re.compile(r"StyleData\{Fields:|packStyle\(Style\{Fields:")
VISUAL_PROP = re.compile(r"^\s*(background|foreground|border|radius|opacity|color)[a-z_]*:", re.I)


def ls_files(*patterns):
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", *patterns],
                         capture_output=True, text=True, check=True).stdout
    return [ROOT / line for line in out.splitlines()]


def text(path):
    return path.read_text(errors="replace")


def line_hits(path, pattern):
    return [i for i, line in enumerate(text(path).splitlines(), 1) if pattern.search(line)]


def require(condition, message, failures):
    if not condition:
        failures.append(message)


def main():
    ledger = text(LEDGER)
    failures = []
    required_ids = [f"B-{i:03d}" for i in (1, 2, 5, 6, 7, 8)]
    for bridge_id in required_ids:
        require(f"| {bridge_id} |" in ledger, f"missing ledger row {bridge_id}", failures)

    checks = {
        "B-001": line_hits(ROOT / "src/ui/frame.kry", CHROME_GETTERS),
        "B-002": line_hits(ROOT / "src/ui/tree_layout.kry", CHROME_GETTERS),
        "B-005": line_hits(ROOT / "go/kryon/control_style_host.go", GO_BASE_PAT),
    }
    for bridge_id, hits in checks.items():
        require(hits, f"{bridge_id} has no matching source hits; update/delete row", failures)

    visual_files = {
        "B-006": ROOT / "runtime/control_props.kry",
        "B-007": ROOT / "runtime/node2d_props.kry",
        "B-008": ROOT / "runtime/reorder_props.kry",
    }
    for bridge_id, path in visual_files.items():
        require(line_hits(path, VISUAL_PROP), f"{bridge_id} has no visual-prop hits; update/delete row", failures)

    visual_count = 0
    for path in ls_files("runtime/*_props.kry"):
        visual_count += len(line_hits(path, VISUAL_PROP))
    require(visual_count == 10, f"visual prop count is {visual_count}, expected ratchet 10", failures)

    if failures:
        for failure in failures:
            print(f"bridge-ledger-check: {failure}", file=sys.stderr)
        return 1
    print("bridge-ledger-check: ok (6 bridge rows, 10 visual props)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
