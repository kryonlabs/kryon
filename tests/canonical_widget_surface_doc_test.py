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
GO_API = ROOT / "go/kryon/api.go"
WEB_RUNTIME = ROOT / "web/kryon-runtime.js"
DOC = ROOT / "docs/CANONICAL_WIDGET_SURFACE.md"
FEATURE_MATRIX = ROOT / "docs/FEATURE_MATRIX.md"
RUNTIME = ROOT / "runtime"

NATIVE_COMPAT_EXPORTS = set()

NATIVE_SCOPE_EXPORT_ALLOWLIST = {
    "BeginTree",
    "End",
    "EndTree",
}

ROLE_COMPAT_EXPORTS = set()

GO_COMPAT_EXPORTS = {
    "BeginDisabled",
    "BeginPopup",
    "BeginScroll",
    "BeginTableCell",
    "EndDisabled",
    "EndPopup",
    "EndScroll",
    "EndTableCell",
    "BeginCanvas",
    "EndCanvas",
}

WEB_COMPAT_ENTRIES = set()

GO_SCOPE_EXPORT_ALLOWLIST = {
    "BeginFrame",
    "End",
    "EndFrame",
}

PUBLIC_WIDGET_NAMES = {
    "Button",
    "Bullet",
    "Drag",
    "DragDrop",
    "Dropdown",
    "Flow",
    "Guide",
    "Heading",
    "Image",
    "Input",
    "Link",
    "Menu",
    "Page",
    "Paragraph",
    "ParagraphText",
    "Popup",
    "Section",
    "Slider",
    "Surface",
    "Toast",
}


def registry_names() -> list[str]:
    text = REGISTRY.read_text(encoding="utf-8")
    return re.findall(r'\{"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"[^"]*"', text)


def runtime_module_paths() -> list[str]:
    return sorted(path.relative_to(ROOT).as_posix() for path in RUNTIME.glob("*.kry"))


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


def ui_tree_function_names() -> set[str]:
    text = UI_TREE.read_text(encoding="utf-8")
    names = set(
        re.findall(
            r"^[A-Za-z_][A-Za-z0-9_ *]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(",
            text,
            flags=re.M,
        )
    )
    if not names:
        raise AssertionError("empty ui_tree.h function list")
    return names


def ui_tree_compat_exports() -> set[str]:
    functions = ui_tree_function_names()
    scope_exports = {
        name
        for name in functions
        if (name.startswith("Begin") or name.startswith("End"))
        and name not in NATIVE_SCOPE_EXPORT_ALLOWLIST
    }
    return scope_exports | (functions & ROLE_COMPAT_EXPORTS)


def go_api_function_names() -> set[str]:
    text = GO_API.read_text(encoding="utf-8")
    names = set(re.findall(r"^func\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(", text, flags=re.M))
    if not names:
        raise AssertionError("empty go/kryon/api.go function list")
    return names


def go_compat_exports() -> set[str]:
    functions = go_api_function_names()
    scope_exports = {
        name
        for name in functions
        if (name.startswith("Begin") or name.startswith("End"))
        and name not in GO_SCOPE_EXPORT_ALLOWLIST
    }
    return scope_exports | (functions & ROLE_COMPAT_EXPORTS)


def web_runtime_names() -> set[str]:
    text = WEB_RUNTIME.read_text(encoding="utf-8")
    names = set(re.findall(r'^export function\s+([A-Z][A-Za-z0-9_]*)\s*\(', text, flags=re.M))
    names.update(re.findall(r'name\s*===\s*"([^"]+)"', text))
    names.update(re.findall(r'case\s+"([^"]+)":', text))
    if not names:
        raise AssertionError("empty web/kryon-runtime.js public name list")
    return names


def web_compat_entries() -> set[str]:
    return web_runtime_names() & WEB_COMPAT_ENTRIES


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


def runtime_module_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Runtime `\.kry` Modules\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Runtime `.kry` Modules section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            module = cells[0].strip("`")
            if module in rows:
                raise AssertionError(f"duplicate runtime module row: {module}")
            rows[module] = cells
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


def go_compat_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Go Public Compatibility Exports\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Go Public Compatibility Exports section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate Go compatibility export row: {name}")
            rows[name] = cells
    return rows


def web_compat_rows() -> dict[str, list[str]]:
    text = DOC.read_text(encoding="utf-8")
    match = re.search(
        r"^## Web Runtime Compatibility Entries\n(?P<body>.*?)(?=^## )",
        text,
        flags=re.M | re.S,
    )
    if not match:
        raise AssertionError("missing ## Web Runtime Compatibility Entries section")

    rows: dict[str, list[str]] = {}
    for line in match.group("body").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[0].startswith("`") and cells[0].endswith("`"):
            name = cells[0].strip("`")
            if name in rows:
                raise AssertionError(f"duplicate web compatibility entry row: {name}")
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
    runtime_expected = runtime_module_paths()
    runtime_doc_rows = runtime_module_rows()
    expected = registry_names()
    rows = audit_rows()
    parser_expected = parser_widget_names()
    parser_doc_rows = parser_rows()
    block_expected = block_widget_names()
    block_doc_rows = block_rows()
    compat_doc_rows = compat_rows()
    go_compat_doc_rows = go_compat_rows()
    web_compat_doc_rows = web_compat_rows()
    feature_count, feature_names = feature_matrix_parser_names()
    doc = DOC.read_text(encoding="utf-8")
    ui_tree_functions = ui_tree_function_names()
    compat_expected = ui_tree_compat_exports()
    go_api_functions = go_api_function_names()
    go_compat_expected = go_compat_exports()
    web_runtime_public_names = web_runtime_names()
    web_compat_expected = web_compat_entries()

    for module in runtime_expected:
        if module not in runtime_doc_rows:
            errors.append(f"missing runtime module row: {module}")
    for module in sorted(set(runtime_doc_rows) - set(runtime_expected)):
        errors.append(f"runtime module row has no file: {module}")
    for name in expected:
        if name not in rows:
            errors.append(f"missing registry audit row: {name}")
    for name in sorted(set(rows) - set(expected)):
        errors.append(f"registry audit row is not in node registry: {name}")
    for name in sorted(PUBLIC_WIDGET_NAMES):
        if f"`{name}`" not in doc:
            errors.append(f"missing public widget surface row: {name}")
    guide_row = re.search(r"^\| `Guide` \| (?P<decision>[^|]+) \| (?P<notes>[^|]+) \|$", doc, re.M)
    if not guide_row:
        errors.append("missing Guide overlay review row")
    else:
        decision = guide_row.group("decision").strip()
        notes = guide_row.group("notes")
        if decision != "`.kry canonical`":
            errors.append(f"Guide must stay .kry canonical, found {decision}")
        if "Guide(GuideProps)" not in notes:
            errors.append("Guide row must name the clean Guide(GuideProps) surface")
    paragraph_node_row = re.search(r"^\| `WIDGET_PARAGRAPH` \| `Paragraph` \| (?P<decision>[^|]+) \|$", doc, re.M)
    if not paragraph_node_row:
        errors.append("missing WIDGET_PARAGRAPH retained node row")
    elif "Rename review" in paragraph_node_row.group("decision"):
        errors.append("Paragraph is canonical rich text; WIDGET_PARAGRAPH must not be in rename review")
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
    if compat_expected != NATIVE_COMPAT_EXPORTS:
        for name in sorted(compat_expected - NATIVE_COMPAT_EXPORTS):
            errors.append(f"unreviewed native compatibility export in ui_tree.h: {name}")
        for name in sorted(NATIVE_COMPAT_EXPORTS - compat_expected):
            errors.append(f"stale native compatibility export no longer in ui_tree.h: {name}")
    for name in sorted(NATIVE_COMPAT_EXPORTS):
        if name not in compat_doc_rows:
            errors.append(f"missing native compatibility export row: {name}")
        if name not in ui_tree_functions:
            errors.append(f"native compatibility export no longer exists in ui_tree.h: {name}")
    for name in sorted(set(compat_doc_rows) - NATIVE_COMPAT_EXPORTS):
        errors.append(f"native compatibility export row is not tracked by the test: {name}")
    if go_compat_expected != GO_COMPAT_EXPORTS:
        for name in sorted(go_compat_expected - GO_COMPAT_EXPORTS):
            errors.append(f"unreviewed Go compatibility export in api.go: {name}")
        for name in sorted(GO_COMPAT_EXPORTS - go_compat_expected):
            errors.append(f"stale Go compatibility export no longer in api.go: {name}")
    for name in sorted(GO_COMPAT_EXPORTS):
        if name not in go_compat_doc_rows:
            errors.append(f"missing Go compatibility export row: {name}")
        if name not in go_api_functions:
            errors.append(f"Go compatibility export no longer exists in api.go: {name}")
    for name in sorted(set(go_compat_doc_rows) - GO_COMPAT_EXPORTS):
        errors.append(f"Go compatibility export row is not tracked by the test: {name}")
    if web_compat_expected != WEB_COMPAT_ENTRIES:
        for name in sorted(web_compat_expected - WEB_COMPAT_ENTRIES):
            errors.append(f"unreviewed web compatibility entry in kryon-runtime.js: {name}")
        for name in sorted(WEB_COMPAT_ENTRIES - web_compat_expected):
            errors.append(f"stale web compatibility entry no longer in kryon-runtime.js: {name}")
    for name in sorted(WEB_COMPAT_ENTRIES):
        if name not in web_compat_doc_rows:
            errors.append(f"missing web compatibility entry row: {name}")
        if name not in web_runtime_public_names:
            errors.append(f"web compatibility entry no longer exists in kryon-runtime.js: {name}")
    for name in sorted(set(web_compat_doc_rows) - WEB_COMPAT_ENTRIES):
        errors.append(f"web compatibility entry row is not tracked by the test: {name}")
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
