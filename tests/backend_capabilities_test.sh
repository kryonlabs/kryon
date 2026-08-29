#!/usr/bin/env sh
set -eu

root="${1:-.}"
cd "$root"

python3 - <<'PY'
import json
import re
import sys
from pathlib import Path

root = Path(".")
manifest_path = root / "docs" / "BACKEND_CAPABILITIES.json"
makefile = (root / "Makefile").read_text(encoding="utf-8")
data = json.loads(manifest_path.read_text(encoding="utf-8"))
backends = data.get("backends", [])
errors = []

ids = [entry.get("id") for entry in backends]
if len(ids) != len(set(ids)):
    errors.append("duplicate backend ids in docs/BACKEND_CAPABILITIES.json")

expected = set(re.findall(r"(?:else )?ifeq \(\$\(KRYON_BACKEND\),([A-Za-z0-9_+-]+)\)", makefile))
error_match = re.search(r"expected ([^)]+)\)", makefile)
if error_match:
    expected.update(
        name for name in re.findall(r"[A-Za-z0-9_+-]+", error_match.group(1))
        if name != "or"
    )

listed = set(ids)
missing = sorted(expected - listed)
extra = sorted(listed - expected)
if missing:
    errors.append("Makefile backends missing from manifest: " + ", ".join(missing))
if extra:
    errors.append("manifest backends are not accepted by Makefile: " + ", ".join(extra))

required_fields = ["id", "label", "tier", "platforms", "surface", "status", "checks"]
for entry in backends:
    for field in required_fields:
        if not entry.get(field):
            errors.append(f"backend {entry.get('id')!r} missing {field}")
    for field in ["platforms", "checks"]:
        if field in entry and not isinstance(entry[field], list):
            errors.append(f"backend {entry.get('id')!r} field {field} must be a list")

if errors:
    for error in errors:
        print(error, file=sys.stderr)
    sys.exit(1)

print(f"backend capabilities ok: {len(backends)} backends")
PY
