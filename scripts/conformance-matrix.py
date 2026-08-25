#!/usr/bin/env python3
"""Generate and verify Kryon's source/pipeline conformance matrix.

The matrix is intentionally mechanical: it scans the checked-in .kry examples
and generated-runtime parity fixtures, records which widget families each file
exercises, and can verify that every listed source lowers through k2ir, k2c,
k2g, and k2b.
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "site" / "conformance-matrix.json"

PIPELINES = [
    {
        "id": "k2ir",
        "label": ".kry -> KIR",
        "tool": "build/linux-x86_64/bin/k2ir",
        "evidence": "tests/k2ir_test.sh plus conformance-matrix-check",
    },
    {
        "id": "k2c",
        "label": ".kry -> KIR -> C",
        "tool": "build/linux-x86_64/bin/k2c",
        "evidence": "tests/k2c_syntax_test.sh plus conformance-matrix-check",
    },
    {
        "id": "k2g",
        "label": ".kry -> KIR -> Go",
        "tool": "build/linux-x86_64/bin/k2g",
        "evidence": "tests/k2g_syntax_test.sh plus conformance-matrix-check",
    },
    {
        "id": "k2b",
        "label": ".kry -> KIR -> KRB",
        "tool": "build/linux-x86_64/bin/k2b",
        "evidence": "tests/krb_cartridge_test.sh plus conformance-matrix-check",
    },
]

RENDERERS = [
    {
        "id": "desktop-raylib",
        "label": "Desktop raylib",
        "platform": "Linux/FreeBSD desktop",
        "approach": "raylib surface backend",
        "status": "gated",
        "status_class": "ok",
        "evidence": ["make test", "tests/raylib_compat_test.c"],
        "notes": "Default native surface backend.",
    },
    {
        "id": "web-raylib-wasm",
        "label": "Web raylib wasm",
        "platform": "Emscripten web",
        "approach": "raylib WebGL surface backend",
        "status": "build-gated",
        "status_class": "part",
        "evidence": ["mk/web.mk", "docs-site Web IDE build"],
        "notes": "Shipping web path; visual matrix still needs the same per-source screenshot gate as KRB.",
    },
    {
        "id": "web-canvas-wasm",
        "label": "Web canvas wasm",
        "platform": "Emscripten web",
        "approach": "HTML5 Canvas2D surface backend",
        "status": "conditional gate",
        "status_class": "part",
        "evidence": ["make canvas-test", "tests/canvas_backend_test.sh", "tests/canvas_audio_test.sh"],
        "notes": "Runs when emcc and node are available.",
    },
    {
        "id": "desktop-libdraw",
        "label": "Desktop libdraw",
        "platform": "Unix/X11 with plan9port",
        "approach": "kry_sw to libdraw/devdraw",
        "status": "conditional gate",
        "status_class": "part",
        "evidence": ["make libdraw-test", "tests/libdraw_backend_test.sh", "tests/libdraw_9c_test.sh"],
        "notes": "Runs when plan9port and a display or xvfb are available.",
    },
    {
        "id": "krb-native-sw",
        "label": "KRB native software",
        "platform": "Native host",
        "approach": "KryBackend + kry_sw",
        "status": "gated",
        "status_class": "ok",
        "evidence": ["tests/krb_engine_test.sh", "tests/kry_sw_test.c"],
        "notes": "Portable cartridge renderer used by preview and exactness checks.",
    },
    {
        "id": "krb-web-canvas",
        "label": "KRB web canvas",
        "platform": "Emscripten web",
        "approach": "kry_sw wasm blitted to Canvas2D",
        "status": "partial",
        "status_class": "part",
        "evidence": ["cmd/krb-web/main.c", "cmd/krb-web/node-capture.js"],
        "notes": "Node capture path exists; full matrix screenshot comparison is not yet applied to every source.",
    },
]

WIDGETS = {
    "ActionModal",
    "Background",
    "Bevel",
    "BottomNav",
    "Button",
    "Canvas",
    "CanvasGrid",
    "Checkbox",
    "Collapsible",
    "ColorPicker",
    "Column",
    "Combobox",
    "ConfirmDialog",
    "Dropdown",
    "Group",
    "Href",
    "IconButton",
    "IconLink",
    "IconTexture",
    "ImageBox",
    "LabelFrame",
    "Line",
    "ListBox",
    "MenuBar",
    "MessageDialog",
    "Modal",
    "Notebook",
    "PanedView",
    "Paragraph",
    "Picture",
    "Progress",
    "PromptDialog",
    "Radio",
    "Rect",
    "Row",
    "Screen",
    "Scroll",
    "SelectableText",
    "Slider",
    "SourceView",
    "Spinbox",
    "Stack",
    "TabBar",
    "TableView",
    "Text",
    "TextArea",
    "TextField",
    "TextInRect",
    "TextLines",
    "TitleBar",
    "Toast",
    "Toggle",
    "Toolbar",
    "TopNav",
    "TreeView",
}

CALL_RE = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\s*\(")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def app_title(source: str, fallback: str) -> str:
    match = re.search(r'\bapp\s+"([^"]+)"', source)
    return match.group(1) if match else fallback


def parity_fixtures() -> set[str]:
    path = ROOT / "tests" / "generated_runtime_parity_test.sh"
    text = read(path)
    match = re.search(r'fixtures="\n(?P<body>.*?)\n"', text, re.S)
    if not match:
        raise SystemExit("could not find fixtures block in generated_runtime_parity_test.sh")
    return {line.strip() for line in match.group("body").splitlines() if line.strip()}


def detect_widgets(source: str) -> list[str]:
    clean = STRING_RE.sub('""', source)
    found = {match.group(1) for match in CALL_RE.finditer(clean)}
    if "BeginCanvas" in found or "EndCanvas" in found:
        found.add("Canvas")
    if "BeginScrollContainer" in found or "EndScrollContainer" in found:
        found.add("Scroll")
    return sorted(found & WIDGETS)


def source_cases() -> list[dict]:
    parity = parity_fixtures()
    paths = sorted((ROOT / "examples").glob("*.kry"))
    paths += sorted((ROOT / "tests" / "parity").glob("*.kry"))
    cases = []
    for path in paths:
        source = read(path)
        r = rel(path)
        case_type = "parity fixture" if r.startswith("tests/parity/") else "example"
        semantic_gate = r in parity
        pipelines = {}
        for pipeline in PIPELINES:
            pipelines[pipeline["id"]] = {
                "status": "gated",
                "status_class": "ok",
                "evidence": "conformance-matrix-check",
            }
        cases.append(
            {
                "id": r.removesuffix(".kry").replace("/", "-"),
                "path": r,
                "label": app_title(source, path.stem.replace("_", " ")),
                "type": case_type,
                "widgets": detect_widgets(source),
                "pipelines": pipelines,
                "semantic_parity": semantic_gate,
                "semantic_evidence": (
                    "tests/generated_runtime_parity_test.sh"
                    if semantic_gate
                    else "lowering-only; no state/visual parity automaton yet"
                ),
            }
        )
    return cases


def matrix() -> dict:
    cases = source_cases()
    widget_counts: dict[str, int] = {}
    semantic_count = 0
    for case in cases:
        semantic_count += int(case["semantic_parity"])
        for widget in case["widgets"]:
            widget_counts[widget] = widget_counts.get(widget, 0) + 1
    return {
        "schema": 1,
        "source": "scripts/conformance-matrix.py",
        "summary": {
            "source_cases": len(cases),
            "examples": sum(1 for case in cases if case["type"] == "example"),
            "parity_fixtures": sum(1 for case in cases if case["type"] == "parity fixture"),
            "pipeline_cells": len(cases) * len(PIPELINES),
            "semantic_parity_cases": semantic_count,
            "widgets_detected": len(widget_counts),
        },
        "pipelines": [
            {
                "id": item["id"],
                "label": item["label"],
                "status": "gated",
                "status_class": "ok",
                "evidence": item["evidence"],
            }
            for item in PIPELINES
        ],
        "renderers": RENDERERS,
        "widget_counts": dict(sorted(widget_counts.items())),
        "cases": cases,
    }


def encode(data: dict) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def check_output(rendered: str) -> int:
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
    if current == rendered:
        return 0
    diff = difflib.unified_diff(
        current.splitlines(),
        rendered.splitlines(),
        fromfile=rel(OUTPUT),
        tofile="generated conformance matrix",
        lineterm="",
    )
    print("docs/site/conformance-matrix.json is stale; run scripts/conformance-matrix.py", file=sys.stderr)
    print("\n".join(diff), file=sys.stderr)
    return 1


def verify_pipelines(data: dict) -> int:
    missing = [p for p in PIPELINES if not (ROOT / p["tool"]).exists()]
    if missing:
        for pipeline in missing:
            print(f"missing tool for {pipeline['id']}: {pipeline['tool']}", file=sys.stderr)
        return 1

    failures = []
    with tempfile.TemporaryDirectory(prefix="kryon-conformance-matrix.") as tmp:
        tmp_path = Path(tmp)
        for case in data["cases"]:
            source = ROOT / case["path"]
            for pipeline in PIPELINES:
                out = tmp_path / pipeline["id"] / case["id"]
                if out.exists():
                    shutil.rmtree(out)
                out.mkdir(parents=True)
                cmd = [str(ROOT / pipeline["tool"]), "--root", str(ROOT), "-o", str(out), str(source)]
                run = subprocess.run(cmd, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if run.returncode != 0:
                    failures.append(
                        {
                            "case": case["path"],
                            "pipeline": pipeline["id"],
                            "error": (run.stderr or run.stdout).strip().splitlines()[:3],
                        }
                    )
    if failures:
        for failure in failures:
            print(f"{failure['case']} failed {failure['pipeline']}", file=sys.stderr)
            for line in failure["error"]:
                print(f"  {line}", file=sys.stderr)
        return 1
    print(f"conformance matrix pipelines ok: {len(data['cases'])} sources x {len(PIPELINES)} pipelines")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated website JSON is current")
    parser.add_argument("--verify-pipelines", action="store_true", help="run all listed sources through k2ir/k2c/k2g/k2b")
    args = parser.parse_args()

    data = matrix()
    rendered = encode(data)
    if args.check:
        rc = check_output(rendered)
        if rc:
            return rc
    elif not args.verify_pipelines:
        OUTPUT.write_text(rendered, encoding="utf-8")
        print(
            f"rendered {rel(OUTPUT)}: "
            f"{data['summary']['source_cases']} sources, "
            f"{data['summary']['pipeline_cells']} pipeline cells"
        )

    if args.verify_pipelines:
        return verify_pipelines(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
