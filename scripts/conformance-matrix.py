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
import struct
import sys
import tempfile
import zlib
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
        "scope": "Surface API and default native runtime gate; per-source PNG matrix not yet implemented.",
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
        "scope": "Build coverage; per-source browser screenshot matrix not yet implemented.",
        "notes": "Shipping web path; visual matrix still needs the same per-source screenshot gate as KRB.",
    },
    {
        "id": "web-canvas-wasm",
        "label": "Web canvas wasm",
        "platform": "Emscripten web",
        "approach": "HTML5 Canvas2D surface backend",
        "status": "smoke-gated",
        "status_class": "part",
        "evidence": ["make renderer-matrix-check", "tests/canvas_backend_test.sh", "tests/canvas_audio_test.sh"],
        "scope": "Wasm+Node Canvas2D command-sequence smoke and WebAudio smoke; per-source PNG matrix not yet implemented.",
        "notes": "Runs when emcc and node are available; scripts skip cleanly when unavailable.",
    },
    {
        "id": "desktop-libdraw",
        "label": "Desktop libdraw",
        "platform": "Unix/X11 with plan9port",
        "approach": "kry_sw to libdraw/devdraw",
        "status": "smoke-gated",
        "status_class": "part",
        "evidence": ["make renderer-matrix-check", "tests/libdraw_backend_test.sh", "tests/libdraw_9c_test.sh"],
        "scope": "Desktop PNG smoke plus clean plan9port 9c/9l link; per-source PNG matrix not yet implemented.",
        "notes": "Runs when plan9port and a display or xvfb are available; scripts skip cleanly when unavailable.",
    },
    {
        "id": "krb-native-sw",
        "label": "KRB native software",
        "platform": "Native host",
        "approach": "KryBackend + kry_sw",
        "status": "gated",
        "status_class": "ok",
        "evidence": ["make conformance-matrix-check", "tests/krb_engine_test.sh", "tests/kry_sw_test.c"],
        "scope": "Per-source RGB visual matrix against SDL readback for every listed .kry source.",
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
        "scope": "Node capture path exists; per-source wasm capture matrix not yet implemented.",
        "notes": "Node capture path exists; full matrix screenshot comparison is not yet applied to every source.",
    },
]

RENDERER_SMOKE_CHECKS = [
    {
        "id": "krb-exact",
        "label": "KRB renderer exactness",
        "command": ["sh", "tests/krb_exact_test.sh", "."],
        "scope": "Curated byte-exact kry_sw vs SDL readback PNG comparison.",
    },
    {
        "id": "canvas-wasm",
        "label": "Canvas wasm backend",
        "command": ["make", "-C", ".", "canvas-test"],
        "scope": "Emscripten Canvas2D command-sequence smoke plus WebAudio smoke.",
    },
    {
        "id": "libdraw-desktop",
        "label": "Libdraw desktop backend",
        "command": ["make", "-C", ".", "libdraw-test"],
        "scope": "plan9port/devdraw PNG smoke plus clean 9c/9l surface link.",
    },
]

KRB_ALPHA_BYTE_GAPS = {
    "examples/20_scene.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/21_signals.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/22_physics.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/23_animation.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/24_tilemap.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/buttons_layout.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/fields.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/focus.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/generated_form.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/long_text.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
}

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
        alpha_gap = KRB_ALPHA_BYTE_GAPS.get(r)
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
                "visuals": {
                    "krb_rgb": {
                        "status": "RGB exact",
                        "status_class": "ok",
                        "evidence": "conformance-matrix-check",
                    },
                    "krb_alpha": {
                        "status": "alpha differs" if alpha_gap else "byte exact",
                        "status_class": "part" if alpha_gap else "ok",
                        "evidence": alpha_gap or "conformance-matrix-check",
                    },
                },
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
            "krb_rgb_visual_cases": len(cases),
            "krb_alpha_byte_exact_cases": len(cases) - len(KRB_ALPHA_BYTE_GAPS),
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
        "renderer_checks": [
            {
                "id": item["id"],
                "label": item["label"],
                "command": " ".join(item["command"]),
                "scope": item["scope"],
            }
            for item in RENDERER_SMOKE_CHECKS
        ],
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


def png_rgba(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    width = height = None
    raw = b""
    while pos < len(data):
        if pos + 8 > len(data):
            raise ValueError(f"{path}: truncated PNG chunk")
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += length + 12
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _comp, _filter, interlace = struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8 or color_type != 6 or interlace != 0:
                raise ValueError(f"{path}: expected non-interlaced 8-bit RGBA PNG")
        elif chunk_type == b"IDAT":
            raw += chunk
        elif chunk_type == b"IEND":
            break
    if width is None or height is None:
        raise ValueError(f"{path}: missing PNG header")

    decoded = zlib.decompress(raw)
    stride = width * 4
    pixels = bytearray()
    prev = bytearray(stride)
    idx = 0
    for _y in range(height):
        filt = decoded[idx]
        idx += 1
        row = bytearray(decoded[idx:idx + stride])
        idx += stride
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            up = prev[x]
            up_left = prev[x - 4] if x >= 4 else 0
            if filt == 1:
                row[x] = (row[x] + left) & 0xff
            elif filt == 2:
                row[x] = (row[x] + up) & 0xff
            elif filt == 3:
                row[x] = (row[x] + ((left + up) // 2)) & 0xff
            elif filt == 4:
                p = left + up - up_left
                pa = abs(p - left)
                pb = abs(p - up)
                pc = abs(p - up_left)
                pred = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                row[x] = (row[x] + pred) & 0xff
            elif filt != 0:
                raise ValueError(f"{path}: unsupported PNG filter {filt}")
        pixels.extend(row)
        prev = row
    return width, height, bytes(pixels)


def rgba_diff(a_path: Path, b_path: Path) -> tuple[int, int, int]:
    aw, ah, ap = png_rgba(a_path)
    bw, bh, bp = png_rgba(b_path)
    if aw != bw or ah != bh:
        raise ValueError(f"image sizes differ: {a_path}={aw}x{ah} {b_path}={bw}x{bh}")
    pixel_diff = 0
    rgb_diff = 0
    alpha_diff = 0
    for i in range(0, len(ap), 4):
        aa = ap[i:i + 4]
        bb = bp[i:i + 4]
        if aa == bb:
            continue
        pixel_diff += 1
        if aa[:3] != bb[:3]:
            rgb_diff += 1
        if aa[3] != bb[3]:
            alpha_diff += 1
    return pixel_diff, rgb_diff, alpha_diff


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


def verify_krb_visuals(data: dict) -> int:
    tools = {
        "k2b": ROOT / "build/linux-x86_64/bin/k2b",
        "krb-run": ROOT / "build/linux-x86_64/bin/krb-run",
        "krb-sdl": ROOT / "build/linux-x86_64/bin/krb-sdl",
    }
    missing = [f"{name}: {path}" for name, path in tools.items() if not path.exists()]
    if missing:
        for item in missing:
            print(f"missing tool for KRB visual check: {item}", file=sys.stderr)
        return 1

    failures = []
    alpha_gaps = set()
    with tempfile.TemporaryDirectory(prefix="kryon-conformance-visual.") as tmp:
        work = Path(tmp)
        for case in data["cases"]:
            source = ROOT / case["path"]
            stem = case["path"].removesuffix(".kry")
            out_dir = work / "krb"
            krb = out_dir / f"{stem}.krb"
            sw_png = work / f"{stem}.sw.png"
            sdl_png = work / f"{stem}.sdl.png"
            sw_png.parent.mkdir(parents=True, exist_ok=True)
            sdl_png.parent.mkdir(parents=True, exist_ok=True)

            run = subprocess.run(
                [str(tools["k2b"]), "--no-main", "--root", str(ROOT), "-o", str(out_dir), str(source)],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                failures.append((case["path"], "k2b", (run.stderr or run.stdout).strip()))
                continue
            run = subprocess.run(
                [str(tools["krb-run"]), "--png", str(sw_png), "--w", "480", "--h", "640", str(krb)],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                failures.append((case["path"], "krb-run", (run.stderr or run.stdout).strip()))
                continue
            env = os.environ.copy()
            env["SDL_VIDEODRIVER"] = "dummy"
            env["SDL_RENDER_DRIVER"] = "software"
            run = subprocess.run(
                [str(tools["krb-sdl"]), "--png", str(sdl_png), "--w", "480", "--h", "640", str(krb)],
                cwd=ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                failures.append((case["path"], "krb-sdl", (run.stderr or run.stdout).strip()))
                continue
            pixel_diff, rgb_diff, alpha_diff = rgba_diff(sw_png, sdl_png)
            if rgb_diff:
                failures.append((case["path"], "rgb", f"{rgb_diff} RGB pixels differ ({pixel_diff} RGBA pixels differ)"))
            elif alpha_diff:
                alpha_gaps.add(case["path"])

    known = set(KRB_ALPHA_BYTE_GAPS)
    for path in sorted(alpha_gaps - known):
        failures.append((path, "alpha", "alpha bytes differ but the matrix does not list this gap"))
    for path in sorted(known - alpha_gaps):
        failures.append((path, "alpha", "listed as alpha-different but now byte-exact"))

    if failures:
        for path, phase, detail in failures:
            print(f"KRB visual check failed for {path} during {phase}", file=sys.stderr)
            if detail:
                print(f"  {detail}", file=sys.stderr)
        return 1
    print(
        f"KRB visual matrix ok: {len(data['cases'])} RGB-exact sources, "
        f"{len(data['cases']) - len(alpha_gaps)} byte-exact, {len(alpha_gaps)} alpha-only gaps"
    )
    return 0


def verify_renderer_smokes() -> int:
    failures = []
    for check in RENDERER_SMOKE_CHECKS:
        command = list(check["command"])
        if command[2] == ".":
            command[2] = str(ROOT)
        run = subprocess.run(
            command,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if run.returncode != 0:
            failures.append((check["id"], run.stdout.strip().splitlines()[-20:]))
        else:
            print(f"renderer smoke ok: {check['id']}")
    if failures:
        for check_id, tail in failures:
            print(f"renderer smoke failed: {check_id}", file=sys.stderr)
            for line in tail:
                print(f"  {line}", file=sys.stderr)
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated website JSON is current")
    parser.add_argument("--verify-pipelines", action="store_true", help="run all listed sources through k2ir/k2c/k2g/k2b")
    parser.add_argument("--verify-krb-visuals", action="store_true", help="compare KRB headless PNGs against SDL readback PNGs")
    parser.add_argument("--verify-renderer-smokes", action="store_true", help="run non-per-source renderer smoke gates")
    args = parser.parse_args()

    data = matrix()
    rendered = encode(data)
    if args.check:
        rc = check_output(rendered)
        if rc:
            return rc
    elif not args.verify_pipelines and not args.verify_krb_visuals and not args.verify_renderer_smokes:
        OUTPUT.write_text(rendered, encoding="utf-8")
        print(
            f"rendered {rel(OUTPUT)}: "
            f"{data['summary']['source_cases']} sources, "
            f"{data['summary']['pipeline_cells']} pipeline cells"
        )

    if args.verify_pipelines:
        rc = verify_pipelines(data)
        if rc:
            return rc
    if args.verify_krb_visuals:
        rc = verify_krb_visuals(data)
        if rc:
            return rc
    if args.verify_renderer_smokes:
        return verify_renderer_smokes()
    return 0


if __name__ == "__main__":
    sys.exit(main())
