#!/usr/bin/env python3
"""Run the native storage/measurement adapter against shared layout fixtures."""

import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / sys.argv[1]).resolve()
generated = BUILD / "generated/src"
output = BUILD / "tests/paragraph_layout_test"
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([
    *shlex.split(os.environ.get("CC", "cc")), "-std=c99", "-Wall", "-Wextra", "-Werror",
    "-I" + str(BUILD / "generated/include"), "-I" + str(generated), "-Iinclude",
    "-Isrc/ui", "-Ivendor/utf8proc", "tests/paragraph_layout_test.c",
    str(generated / "ui/text_layout.c"),
    str(generated / "runtime/paragraph.c"), "-lm", "-o", str(output),
], cwd=ROOT, check=True)
cases = json.loads((ROOT / "tests/fixtures/paragraph_layout.json").read_text())
for case in cases:
    result = subprocess.run([str(output), case["text"], str(case["width"])],
                            text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f'{case["name"]}: native adapter failed\n{result.stderr}')
    actual = result.stdout.splitlines()
    if actual != case["lines"]:
        raise AssertionError(f'{case["name"]}: got {actual!r}, want {case["lines"]!r}')
print(f"native paragraph layout: {len(cases)} shared fixtures passed")
