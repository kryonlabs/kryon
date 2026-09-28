#!/usr/bin/env python3
"""The handwritten browser JS may only shrink, and must not regain policy.

Kryon's browser hosts are moving to Ziran: JavaScript keeps only raw Web API
effects. tests/web_js_budget.json records the size the JS may not exceed and
identifiers that mark policy (key tables, tag choice) which must live in Zi.
Lower the budget when JS is removed; never raise it.
"""
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent.parent
budget = json.loads((root / "tests/web_js_budget.json").read_text())
failures = []
seen = set()
for path in sorted((root / "web").glob("*.js")):
    seen.add(path.name)
    text = path.read_text()
    lines = text.count("\n") + 1
    entries = len(re.findall(r"(?m)^\s*\$?js_[A-Za-z0-9_]+\s*:\s*(?:function|\()", text))
    allowed = budget["files"].get(path.name)
    if allowed is None:
        failures.append(f"{path.name}: new JS file; browser hosts are written in Ziran")
        continue
    if lines > allowed["lines"]:
        failures.append(f"{path.name}: {lines} lines exceeds budget {allowed['lines']}")
    if entries > allowed["entries"]:
        failures.append(f"{path.name}: {entries} entry points exceeds budget {allowed['entries']}")
    for name in budget["forbidden"]:
        if re.search(r"\b" + re.escape(name) + r"\b", text):
            failures.append(f"{path.name}: policy identifier {name} belongs in Zi")
for name in budget["files"]:
    if name not in seen and budget["files"][name]["lines"] != 0:
        failures.append(f"{name}: removed; set its budget to 0 lines")

if failures:
    print("\n".join(failures), file=sys.stderr)
    sys.exit(1)
total = sum(f["lines"] for f in budget["files"].values())
print(f"Browser JS within budget ({total} lines allowed)")
