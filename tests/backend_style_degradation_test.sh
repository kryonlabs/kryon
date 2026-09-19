#!/usr/bin/env sh
set -eu

root="${1:-.}"
cd "$root"

python3 - <<'PY'
import json
import sys
from pathlib import Path

manifest = Path("docs/BACKEND_CAPABILITIES.json")
data = json.loads(manifest.read_text(encoding="utf-8"))
backends = data.get("backends", [])
errors = []

for entry in backends:
    backend_id = entry.get("id", "<missing>")
    degradation = entry.get("style_degradation")
    if not isinstance(degradation, dict):
        errors.append(f"{backend_id}: missing style_degradation object")
        continue
    for field in ("material", "effects", "reporting"):
        value = degradation.get(field)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"{backend_id}: style_degradation.{field} must be non-empty text")
    text = " ".join(str(degradation.get(field, "")).lower() for field in ("material", "effects", "reporting"))
    if backend_id in {"null", "canvas", "dom", "termi"}:
        if not any(word in text for word in ("degrade", "degradation", "unsupported", "no-op", "ignored")):
            errors.append(f"{backend_id}: constrained backend must describe material/effect degradation")
        if "resolved" not in text or "color" not in text:
            errors.append(f"{backend_id}: degradation must promise stable resolved colors/style data")
        if "substitut" in text and "without" not in text:
            errors.append(f"{backend_id}: pack substitution must be explicitly forbidden when mentioned")
    if backend_id == "raylib" and "full" not in text:
        errors.append("raylib: full material/effect support must be explicit")

if errors:
    for error in errors:
        print(error, file=sys.stderr)
    sys.exit(1)

print(f"backend style degradation ok: {len(backends)} backends")
PY
