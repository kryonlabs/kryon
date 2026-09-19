#!/bin/sh
set -eu

root="${1:-.}"
cd "$root"

sh tests/backend_capabilities_test.sh .
sh tests/backend_style_degradation_test.sh .

python3 - <<'PY'
import json
import sys
from pathlib import Path

manifest = Path("docs/BACKEND_CAPABILITIES.json")
data = json.loads(manifest.read_text(encoding="utf-8"))
backends = data.get("backends", [])
errors = []

required_ids = {"raylib", "null", "canvas", "dom", "libdraw", "termi"}
ids = {entry.get("id") for entry in backends}
missing = sorted(required_ids - ids)
if missing:
    errors.append("required backend capability entries missing: " + ", ".join(missing))

for entry in backends:
    backend_id = entry.get("id", "<missing>")
    surface = entry.get("surface", "").lower()
    checks = entry.get("checks", [])
    if not isinstance(checks, list) or not checks:
        errors.append(f"{backend_id}: checks must be a non-empty list")
    if backend_id in {"null", "canvas", "dom", "termi"} and "null stub" not in surface and "zero-return stubs" not in surface:
        errors.append(f"{backend_id}: unsupported surface behavior must be explicit")
    if backend_id == "null" and "headless" not in entry.get("tier", "").lower() and "headless" not in surface:
        errors.append("null: must be explicitly documented as headless")
    if backend_id == "raylib" and "raylib" not in surface:
        errors.append("raylib: surface must document raylib ownership")

if errors:
    for error in errors:
        print(error, file=sys.stderr)
    sys.exit(1)

print(f"backend capability laws ok: {len(backends)} backends")
PY
