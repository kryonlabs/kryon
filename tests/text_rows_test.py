#!/usr/bin/env python3
"""Execute the Kry text-row host against the same fixtures as native Go."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = (root / sys.argv[1]).resolve()
generated = build / "generated/src"
output = build / "tests/text_rows_test"
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([
    *shlex.split(os.environ.get("CC", "cc")), "-std=c99", "-Wall", "-Wextra", "-Werror",
    "-I" + str(build / "generated/include"), "-I" + str(generated), "-Iinclude", "-Isrc/ui", "-Ivendor/utf8proc", "-DUTF8PROC_STATIC",
    "tests/text_rows_test.c", str(generated / "ui/text_rows.c"), str(generated / "ui/grapheme.c"), "src/backend/kry_unicode.c",
    str(generated / "runtime/text_rows.c"), "-lm", "-o", str(output),
], cwd=root, check=True)
cases = json.loads((root / "tests/fixtures/text_rows.json").read_text())
cases.extend([
    {"name": "large measured rows", "text": "a" * 2600, "width": 1200,
     "words": False, "rows": [[0, 1200, 16], [1200, 2400, 16], [2400, 2600, 16]]},
    {"name": "large cluster after a wrap", "text": "abx" + "\u0301" * 600,
     "width": 1, "words": False, "rows": [[0, 1, 16], [1, 2, 16], [2, 1203, 16]]},
])
for case in cases:
    result = subprocess.run([str(output), case["text"], str(case["width"]), str(int(case["words"]))],
                            text=True, capture_output=True, check=True)
    actual = [[int(x) for x in row.split()] for row in result.stdout.splitlines()]
    assert actual == case["rows"], (case["name"], actual, case["rows"])
print(f"native text rows: {len(cases)} shared fixtures passed")
