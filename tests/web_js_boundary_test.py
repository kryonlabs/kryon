#!/usr/bin/env python3
"""Kryon's browser targets contain no handwritten JavaScript.

Every browser host (canvas, DOM, audio, input, files) is written in Ziran and
reaches the page through Ziran's generic Web bridge, whose one JavaScript file
lives in the Ziran repository. Only test fixtures and the documentation site
may hold JavaScript here, and no build rule may link a JS library from Kryon.
"""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
tracked = subprocess.run(["git", "ls-files"], cwd=root, check=True,
                         capture_output=True, text=True).stdout.split()
failures = []
for name in tracked:
    if not re.search(r"\.(js|mjs|cjs|ts)$", name):
        continue
    if name.startswith(("tests/", "docs/")):
        continue
    failures.append(f"{name}: browser hosts are written in Ziran, not JavaScript")
for name in tracked:
    if not name.startswith("mk/") and name != "Makefile":
        continue
    for number, line in enumerate((root / name).read_text().splitlines(), 1):
        if "--js-library" in line and "KRYON_DIR" in line:
            failures.append(f"{name}:{number}: links a JavaScript library from Kryon")
if failures:
    print("\n".join(failures), file=sys.stderr)
    sys.exit(1)
print("Kryon contains no handwritten browser JavaScript")
