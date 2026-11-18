#!/usr/bin/env python3
"""Kryon's browser targets contain no handwritten JavaScript.

Every browser host (canvas, DOM, audio, input, files) is written in Ziran and
reaches the page through Ziran's generic Web bridge, whose one JavaScript file
lives in the Ziran repository. Only test fixtures and the documentation site
may hold JavaScript here, and no build rule may link a JS library from Kryon.
"""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
paths = os.fsdecode(subprocess.check_output(
    ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
    cwd=root,
)).split("\0")
files = sorted({name for name in paths if (root / name).is_file()})
failures = []
for name in files:
    if not re.search(r"\.(js|mjs|cjs|ts)$", name):
        continue
    if name.startswith(("tests/", "docs/")):
        continue
    failures.append(f"{name}: browser hosts are written in Ziran, not JavaScript")
for name in files:
    if not name.startswith("mk/") and name != "Makefile":
        continue
    for number, line in enumerate((root / name).read_text().splitlines(), 1):
        if "--js-library" in line and "KRYON_DIR" in line:
            failures.append(f"{name}:{number}: links a JavaScript library from Kryon")
if failures:
    print("\n".join(failures), file=sys.stderr)
    sys.exit(1)
print("Kryon contains no handwritten browser JavaScript")
