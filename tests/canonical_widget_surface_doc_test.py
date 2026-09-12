#!/usr/bin/env python3
"""Verify the canonical widget surface document mirrors the public registry."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "src/ui/ui_node_registry.c"
PARSER = ROOT / "cmd/kir/kir_parse.c"
UI_TREE = ROOT / "include/ui_tree.h"
DOC = ROOT / "docs/CANONICAL_WIDGET_SURFACE.md"
FEATURE_MATRIX = ROOT / "docs/FEATURE_MATRIX.md"

NATIVE_COMPAT_EXPORTS = {
    "BeginButton",
    "BeginCard",
    "BeginDisabled",
    "BeginPopup",
    "BeginScroll",
    "BeginTableCell",
    "ContextMenu",
    "DragDropSource",
    "DragDropTarget",
    "EndDisabled",
    "EndPopup",
    "EndScroll",
    "EndTableCell",
    "InvisibleButton",
    "MenuBar",
    "PopupMenu",
}

PUBLIC_WIDGET_NAMES = {
    "BeginButton",
    "BeginCard",
    "BeginDisabled",
    "Bullet",
    "DismissibleOverlay",
    "EndDisabled",
    "Flow",
    "Heading",
    "Page",
    "ParagraphText",
    "Section",
    "ShowToast",
    "ShowToastFor",
    "Surface",
}


def registry_names() -> list[str]:
    text = REGISTRY.read_text(encoding="utf-8")
    return re.findall(r'\{"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"[^"]*"', text)


def parser_widget_names() -> list[str]:
    text = PARSER.read_text(encoding="utf-8")
    match = re.search(
        r"static const char \*const widgets\[\]\s*=\s*\{(?P<body>.*?)\};",
        text,
        flags=re.S,
    )
    if not match:
        raise AssertionError("missing parse_widget_statement widget whitelist")
    names = re.findall(r'"([^"]+)"', match.group("body"))
    if not names:
        raise AssertionError("empty parse_widget_statement widget whitelist")
    return names


def block_widget_names() -> list[str]:
    text = PARSER.read_text(encoding="utf-8")
    match = re.search(
        r"static const char \*\nui_block_prop_type\(const char \*widget\)\n\{(?P<body>.*?)\n\}",
        text,
        flags=re.S,
    )
    if not match:
        raise AssertionError("missing ui_block_prop_type block widget list")
    names: list[str] = []
    for name in re.findall(r'strcmp\(widget,\s*"([^"]+)"\)', match.group("body")):
        if name not in names:
            names.append(name)
    if not names:
        raise AssertionError("empty ui_block_prop_type block widget list")
    return names


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


def parser_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Parser Statement Surface\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Parser Statement Surface section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate parser statement surface row: {name}")
            rows[name] = cells
    return rows


def block_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Block Statement Surface\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Block Statement Surface section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate block statement surface row: {name}")
            rows[name] = cells
    return rows


def compat_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Native Public Compatibility Exports\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Native Public Compatibility Exports section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate native compatibility export row: {name}")
            rows[name] = cells
    return rows


def feature_matrix_parser_names() -> tuple[int, list[str]]:
    text = FEATURE_MATRIX.read_text(encoding="utf-8")
    count_match = re.search(
        r"parse_widget_statement`\s*\(`cmd/kir/kir_parse\.c`\) recognizes (\d+) widget names\.",
        text,
    )
    if not count_match:
        raise AssertionError("missing feature matrix parser widget count")
    list_match = re.search(
        r"operations; and `k2b` lowers a subset of it:\n\n`(?P<body>.*?)`",
        text,
        flags=re.S,
    )
    if not list_match:
        raise AssertionError("missing feature matrix parser widget list")
    names = list_match.group("body").split()
    if not names:
        raise AssertionError("empty feature matrix parser widget list")
    return int(count_match.group(1)), names


def main() -> int:
    errors: list[str] = []
    expected = registry_names()
    rows = audit_rows()
    parser_expected = parser_widget_names()
    parser_doc_rows = parser_rows()
    block_expected = block_widget_names()
    block_doc_rows = block_rows()
    compat_doc_rows = compat_rows()
    feature_count, feature_names = feature_matrix_parser_names()
    doc = DOC.read_text(encoding="utf-8")
    ui_tree = UI_TREE.read_text(encoding="utf-8")

    for name in expected:
        if name not in rows:
            errors.append(f"missing registry audit row: {name}")
    for name in sorted(set(rows) - set(expected)):
        errors.append(f"registry audit row is not in node registry: {name}")
    for name in sorted(PUBLIC_WIDGET_NAMES):
        if f"`{name}`" not in doc:
            errors.append(f"missing public widget surface row: {name}")
    for name in parser_expected:
        if name not in parser_doc_rows:
            errors.append(f"missing parser statement surface row: {name}")
    for name in sorted(set(parser_doc_rows) - set(parser_expected)):
        errors.append(f"parser statement row is not in parse_widget_statement: {name}")
    for name in block_expected:
        if name not in block_doc_rows:
            errors.append(f"missing block statement surface row: {name}")
    for name in sorted(set(block_doc_rows) - set(block_expected)):
        errors.append(f"block statement row is not in ui_block_prop_type: {name}")
    for name in sorted(NATIVE_COMPAT_EXPORTS):
        if name not in compat_doc_rows:
            errors.append(f"missing native compatibility export row: {name}")
        if not re.search(rf"\b{name}\s*\(", ui_tree):
            errors.append(f"native compatibility export no longer exists in ui_tree.h: {name}")
    for name in sorted(set(compat_doc_rows) - NATIVE_COMPAT_EXPORTS):
        errors.append(f"native compatibility export row is not tracked by the test: {name}")
    if feature_count != len(parser_expected):
        errors.append(
            "feature matrix parser widget count is "
            f"{feature_count}, expected {len(parser_expected)}"
        )
    if feature_names != parser_expected:
        errors.append("feature matrix parser widget list does not match parse_widget_statement")

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
