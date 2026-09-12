#!/usr/bin/env python3
"""Verify the canonical widget surface document mirrors the public registry."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "src/ui/ui_node_registry.c"
DOC = ROOT / "docs/CANONICAL_WIDGET_SURFACE.md"


def registry_names() -> list[str]:
    text = REGISTRY.read_text(encoding="utf-8")
    return re.findall(r'\{"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"[^"]*"', text)


def audit_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Registry Surface Audit\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Registry Surface Audit section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 6 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate registry audit row: {name}")
            rows[name] = cells
    return rows


def main() -> int:
    errors: list[str] = []
    expected = registry_names()
    rows = audit_rows()

    for name in expected:
        if name not in rows:
            errors.append(f"missing registry audit row: {name}")
    for name in sorted(set(rows) - set(expected)):
        errors.append(f"registry audit row is not in node registry: {name}")

    for name, cells in rows.items():
        runtime_source = cells[3]
        state = cells[4]
        modules = re.findall(r"`(runtime/[^`]+\.kry)`", runtime_source)
        if ".kry-backed" in state and not modules:
            errors.append(f"{name}: .kry-backed state needs runtime/*.kry source")
        for module in modules:
            if not (ROOT / module).exists():
                errors.append(f"{name}: missing runtime source {module}")

    if errors:
        print("canonical widget surface doc drift:")
        print("\n".join(errors))
        return 1
    print("canonical widget surface doc ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
