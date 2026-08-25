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
import shlex
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
        "status": "source-capture-gated",
        "status_class": "part",
        "evidence": ["make raylib-matrix-check", "tests/generated_c_capture_main.c", "tests/raylib_compat_test.c"],
        "scope": "Per-source .kry -> KIR -> C compile and raylib screenshot capture; cross-renderer pixel diff still pending.",
        "notes": "Default native surface backend. Runs when Xvfb/display is available; scripts skip cleanly when unavailable.",
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
        "status": "source-capture-gated",
        "status_class": "part",
        "evidence": ["make web-canvas-matrix-check", "tests/canvas_generated_c_capture_run.js", "tests/canvas_backend_test.sh"],
        "scope": "Per-source .kry -> KIR -> C -> wasm Canvas2D capture in Node; browser screenshot and cross-renderer pixel diff still pending.",
        "notes": "Runs when emcc and node are available; scripts skip cleanly when unavailable.",
    },
    {
        "id": "desktop-libdraw",
        "label": "Desktop libdraw",
        "platform": "Unix/X11 with plan9port",
        "approach": "kry_sw to libdraw/devdraw",
        "status": "source-capture-gated",
        "status_class": "part",
        "evidence": ["make libdraw-matrix-check", "tests/generated_c_capture_main.c", "tests/libdraw_backend_test.sh"],
        "scope": "Per-source .kry -> KIR -> C compile and libdraw screenshot capture; cross-renderer pixel diff still pending.",
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
        "status": "source-gated",
        "status_class": "ok",
        "evidence": ["make krb-web-matrix-check", "cmd/krb-web/node-capture.js"],
        "scope": "Per-source wasm Canvas2D blit capture byte-compared against native kry_sw output when emcc and node are available.",
        "notes": "Uses the same checked-in .kry matrix sources as the native KRB renderer.",
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
        "id": "web-canvas-c-source-capture",
        "label": "Web Canvas generated-C source capture",
        "command": ["make", "-C", ".", "web-canvas-matrix-check"],
        "scope": "Lowers every matrix .kry source through k2c, links the generated C app host to the wasm Canvas2D backend, captures a PNG frame in Node, and rejects blank output when emcc and node are available.",
    },
    {
        "id": "libdraw-desktop",
        "label": "Libdraw desktop backend",
        "command": ["make", "-C", ".", "libdraw-test"],
        "scope": "plan9port/devdraw PNG smoke plus clean 9c/9l surface link.",
    },
    {
        "id": "raylib-c-source-capture",
        "label": "Raylib generated-C source capture",
        "command": ["make", "-C", ".", "raylib-matrix-check"],
        "scope": "Lowers every matrix .kry source through k2c, links the generated C app host against raylib, captures a PNG frame, and rejects blank output when Xvfb/display is available.",
    },
    {
        "id": "libdraw-c-source-capture",
        "label": "Libdraw generated-C source capture",
        "command": ["make", "-C", ".", "libdraw-matrix-check"],
        "scope": "Lowers every matrix .kry source through k2c, links the generated C app host against libdraw, captures a PNG frame, and rejects blank output when plan9port and xvfb/display are available.",
    },
    {
        "id": "krb-web-canvas",
        "label": "KRB web canvas per-source capture",
        "command": ["make", "-C", ".", "krb-web-matrix-check"],
        "scope": "Builds the KRB web wasm host and byte-compares each matrix source against native kry_sw output when emcc and node are available.",
    },
    {
        "id": "visual-comparison-matrix",
        "label": "Visual comparison matrix",
        "command": ["make", "-C", ".", "visual-comparison-matrix-check"],
        "scope": "Verifies the website comparison table separates pixel/byte-equivalent renderer pairs from capture-only and build-only renderer gaps.",
    },
]

SOURCE_RENDERERS = [
    {
        "id": "desktop_raylib",
        "label": "Desktop raylib",
        "status": "captured",
        "status_class": "ok",
        "evidence": "make raylib-matrix-check",
        "scope": "Per-source generated-C raylib capture; cross-renderer pixel diff pending.",
    },
    {
        "id": "web_raylib_wasm",
        "label": "Web raylib",
        "status": "build gated",
        "status_class": "part",
        "evidence": "docs-site Web IDE build",
        "scope": "Wasm/WebGL build coverage; per-source browser screenshot capture pending.",
    },
    {
        "id": "web_canvas_wasm",
        "label": "Web canvas",
        "status": "captured",
        "status_class": "ok",
        "evidence": "make web-canvas-matrix-check",
        "scope": "Per-source generated-C wasm Canvas2D capture in Node; browser screenshot and cross-renderer pixel diff pending.",
    },
    {
        "id": "krb_native_sw",
        "label": "KRB native",
        "status": "byte exact",
        "status_class": "ok",
        "evidence": "conformance-matrix-check",
        "scope": "Per-source native kry_sw output compared with SDL readback.",
    },
    {
        "id": "krb_web_canvas",
        "label": "KRB web",
        "status": "byte exact",
        "status_class": "ok",
        "evidence": "make krb-web-matrix-check",
        "scope": "Per-source wasm Canvas2D capture byte-compared against native kry_sw.",
    },
    {
        "id": "desktop_libdraw_c",
        "label": "Libdraw C",
        "status": "captured",
        "status_class": "ok",
        "evidence": "make libdraw-matrix-check",
        "scope": "Per-source generated-C libdraw capture; cross-renderer pixel diff pending.",
    },
]

RUNTIME_PARITY_CHECKS = [
    {
        "id": "generated-go-c",
        "label": "Generated Go/C runtime parity",
        "command": ["make", "-C", ".", "generated-runtime-parity-test"],
        "scope": "Lowers parity fixtures through k2g and k2c, drives matching workflows, renders frames, and compares final state JSON.",
    },
]

DOWNSTREAM_CHECKS = [
    {
        "id": "kapsule",
        "label": "Kapsule terminal app",
        "command": ["make", "-C", "../kapsule", "test"],
        "scope": "Builds and runs Kapsule against this Kryon checkout as a real downstream terminal consumer.",
        "optional_dir": "../kapsule",
    },
]

KRB_WEB_SRCS = [
    "cmd/krb-web/main.c",
    "src/krb/krb.c",
    "src/krb/krb_caps.c",
    "src/backend/kry_backend.c",
    "src/backend/kry_sw.c",
    "src/backend/kry_sw_png.c",
    "src/backend/kry_backend_rec.c",
]

KRB_WEB_EXPORTED_FUNCTIONS = (
    "_krb_web_mouse,_krb_web_button,_krb_web_wheel,_krb_web_text,"
    "_krb_web_start,_main"
)

KRB_ALPHA_BYTE_GAPS = {
    "examples/20_scene.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/21_signals.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/22_physics.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/23_animation.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "examples/24_tilemap.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/buttons_layout.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/basic_controls.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/fields.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/focus.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/generated_form.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/list_box.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/long_text.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
    "tests/parity/table_view.kry": "RGB exact; SDL readback alpha differs from headless kry_sw",
}

GENERATED_C_COMPILE_GAPS = {
    "examples/20_scene.kry": "Generated-C app-host route is missing for scene-only source.",
    "examples/14_canvas.kry": "Generated C still has a Canvas lowering gap.",
    "examples/21_signals.kry": "Generated-C app-host route is missing for scene-only source.",
    "examples/22_physics.kry": "Generated-C app-host route is missing for scene-only source.",
    "examples/23_animation.kry": "Generated-C app-host route is missing for scene-only source.",
    "examples/24_tilemap.kry": "Generated-C app-host route is missing for scene-only source.",
    "examples/26_inbe_whm_session.kry": "Generated C still emits unsupported TIME/AnimNode references.",
    "tests/parity/widget_catalog.kry": "Generated C still misses several advanced widget prop shapes.",
}

RAYLIB_C_RENDER_GAPS = {
    "examples/01_file_dialog.kry": "Raylib generated-C capture is blank.",
    "examples/02_buttons.kry": "Raylib generated-C capture is blank.",
    "examples/04_modal.kry": "Raylib generated-C capture is blank.",
    "examples/09_geometry.kry": "Raylib generated-C capture is blank.",
    "examples/11_basic_controls.kry": "Raylib generated-C capture is blank.",
    "examples/17_keyboard_platform.kry": "Raylib generated-C capture is blank.",
    "examples/20_inbe_language.kry": "Raylib generated-C capture is blank.",
    "examples/21_inbe_settings.kry": "Raylib generated-C capture is blank.",
    "examples/22_inbe_manual.kry": "Raylib generated-C capture is blank.",
    "examples/23_inbe_app.kry": "Raylib generated-C capture is blank.",
    "examples/24_inbe_habits.kry": "Raylib generated-C capture is blank.",
    "examples/25_inbe_practice.kry": "Raylib generated-C capture is blank.",
    "examples/28_inbe_profile.kry": "Raylib generated-C capture is blank.",
    "tests/parity/fields.kry": "Raylib generated-C capture is blank.",
    "tests/parity/focus.kry": "Raylib generated-C capture is blank.",
}

LIBDRAW_C_RENDER_GAPS = {
    "examples/01_file_dialog.kry": "Libdraw generated-C capture is blank.",
    "examples/02_buttons.kry": "Libdraw generated-C capture is blank.",
    "examples/04_modal.kry": "Libdraw generated-C capture is blank.",
    "examples/09_geometry.kry": "Libdraw generated-C capture is blank.",
    "examples/11_basic_controls.kry": "Libdraw generated-C capture is blank.",
    "examples/17_keyboard_platform.kry": "Libdraw generated-C capture is blank.",
    "examples/20_inbe_language.kry": "Libdraw generated-C capture is blank.",
    "examples/22_inbe_manual.kry": "Libdraw generated-C capture is blank.",
    "examples/23_inbe_app.kry": "Libdraw generated-C capture is blank.",
    "examples/24_inbe_habits.kry": "Libdraw generated-C capture is blank.",
    "examples/25_inbe_practice.kry": "Libdraw generated-C capture is blank.",
    "examples/28_inbe_profile.kry": "Libdraw generated-C capture is blank.",
}

WEB_CANVAS_C_RENDER_GAPS = {
    "examples/01_file_dialog.kry": "Web Canvas generated-C capture is blank.",
    "examples/02_buttons.kry": "Web Canvas generated-C capture is blank.",
    "examples/03_theme.kry": "Web Canvas generated-C capture traps in wasm.",
    "examples/04_modal.kry": "Web Canvas generated-C capture is blank.",
    "examples/09_geometry.kry": "Web Canvas generated-C capture is blank.",
    "examples/11_basic_controls.kry": "Web Canvas generated-C capture is blank.",
    "examples/17_keyboard_platform.kry": "Web Canvas generated-C capture is blank.",
    "examples/20_inbe_language.kry": "Web Canvas generated-C capture is blank.",
    "examples/21_inbe_settings.kry": "Web Canvas generated-C capture is blank.",
    "examples/22_inbe_manual.kry": "Web Canvas generated-C capture is blank.",
    "examples/23_inbe_app.kry": "Web Canvas generated-C capture is blank.",
    "examples/24_inbe_habits.kry": "Web Canvas generated-C capture is blank.",
    "examples/25_inbe_practice.kry": "Web Canvas generated-C capture is blank.",
    "examples/28_inbe_profile.kry": "Web Canvas generated-C capture is blank.",
    "tests/parity/fields.kry": "Web Canvas generated-C capture is blank.",
    "tests/parity/focus.kry": "Web Canvas generated-C capture is blank.",
    "tests/parity/long_text.kry": "Web Canvas generated-C capture is blank.",
}

LIBDRAW_C_VISUAL_GAPS = {
    **GENERATED_C_COMPILE_GAPS,
    **LIBDRAW_C_RENDER_GAPS,
}
RAYLIB_C_VISUAL_GAPS = {
    **GENERATED_C_COMPILE_GAPS,
    **RAYLIB_C_RENDER_GAPS,
}
WEB_CANVAS_C_VISUAL_GAPS = {
    **GENERATED_C_COMPILE_GAPS,
    **WEB_CANVAS_C_RENDER_GAPS,
}


def visual_comparisons(source_count: int) -> list[dict]:
    definitions = [
        {
            "id": "krb-native-vs-sdl",
            "label": "KRB native software vs SDL host",
            "mode": "RGB pixel comparison",
            "status": "RGB exact",
            "status_class": "ok",
            "evidence": ["make conformance-matrix-check", "tests/krb_exact_test.sh"],
            "scope": "Every listed .kry source is rendered through kry_sw headless and the SDL host; RGB pixels must match. Alpha-only gaps are listed separately.",
            "gap_set": KRB_ALPHA_BYTE_GAPS,
            "gap_label": "alpha-only gaps",
            "compared": source_count,
        },
        {
            "id": "krb-native-vs-web-canvas",
            "label": "KRB native software vs KRB web Canvas",
            "mode": "RGBA byte comparison",
            "status": "byte exact",
            "status_class": "ok",
            "evidence": ["make krb-web-matrix-check", "cmd/krb-web/node-capture.js"],
            "scope": "Every listed .kry source is rendered by native kry_sw and the wasm Canvas2D KRB host; RGBA bytes must match.",
            "gap_set": {},
            "gap_label": "gaps",
            "compared": source_count,
        },
        {
            "id": "raylib-c-vs-krb-native",
            "label": "Generated C raylib vs KRB native software",
            "mode": "capture only",
            "status": "pixel diff pending",
            "status_class": "part",
            "evidence": ["make raylib-matrix-check"],
            "scope": "Generated C raylib captures are gated for nonblank output on sources without known generated-C gaps; pixel comparison against KRB is not yet enforced.",
            "gap_set": RAYLIB_C_VISUAL_GAPS,
            "gap_label": "generated-C gaps",
            "compared": source_count - len(RAYLIB_C_VISUAL_GAPS),
        },
        {
            "id": "libdraw-c-vs-krb-native",
            "label": "Generated C libdraw vs KRB native software",
            "mode": "capture only",
            "status": "pixel diff pending",
            "status_class": "part",
            "evidence": ["make libdraw-matrix-check"],
            "scope": "Generated C libdraw captures are gated for nonblank output on sources without known generated-C gaps; pixel comparison against KRB is not yet enforced.",
            "gap_set": LIBDRAW_C_VISUAL_GAPS,
            "gap_label": "generated-C gaps",
            "compared": source_count - len(LIBDRAW_C_VISUAL_GAPS),
        },
        {
            "id": "web-canvas-c-vs-krb-native",
            "label": "Generated C web Canvas vs KRB native software",
            "mode": "capture only",
            "status": "pixel diff pending",
            "status_class": "part",
            "evidence": ["make web-canvas-matrix-check"],
            "scope": "Generated C wasm Canvas2D captures are gated for nonblank output in Node on sources without known generated-C gaps; browser screenshot and pixel comparison against KRB are not yet enforced.",
            "gap_set": WEB_CANVAS_C_VISUAL_GAPS,
            "gap_label": "generated-C gaps",
            "compared": source_count - len(WEB_CANVAS_C_VISUAL_GAPS),
        },
        {
            "id": "web-raylib-vs-native-raylib",
            "label": "Web raylib wasm vs desktop raylib",
            "mode": "build only",
            "status": "browser screenshot pending",
            "status_class": "part",
            "evidence": ["make docs-site"],
            "scope": "The web raylib path builds with the website/Web IDE, but per-source browser screenshots and pixel comparison against desktop raylib are still missing.",
            "gap_set": {},
            "gap_label": "not screenshot-compared",
            "compared": 0,
        },
    ]
    rows = []
    for row in definitions:
        gap_set = row.pop("gap_set")
        rows.append(
            {
                **row,
                "source_cases": source_count,
                "gap_cases": len(gap_set),
                "gap_label": row["gap_label"],
            }
        )
    return rows


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

WIDGET_ALIASES = {
    "Toast": {"ShowToast", "ShowToastFor"},
}

CALL_RE = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\s*\(")
BLOCK_RE = re.compile(r"\b(Screen|Column|Row|Stack)\s+[A-Za-z_][A-Za-z0-9_]*\s*:")
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
    found.update(match.group(1) for match in BLOCK_RE.finditer(clean))
    if "BeginCanvas" in found or "EndCanvas" in found:
        found.add("Canvas")
    if "BeginUIScrollContainer" in found or "EndUIScrollContainer" in found:
        found.add("Scroll")
    for widget, aliases in WIDGET_ALIASES.items():
        if aliases & found:
            found.add(widget)
    return sorted(found & WIDGETS)


def source_renderer_status(
    renderer: dict,
    source_path: str,
    alpha_gap: str | None,
    raylib_gap: str | None,
    libdraw_gap: str | None,
    web_canvas_gap: str | None,
) -> dict:
    status = {
        "status": renderer["status"],
        "status_class": renderer["status_class"],
        "evidence": renderer["evidence"],
        "scope": renderer["scope"],
    }
    if renderer["id"] == "krb_native_sw" and alpha_gap:
        status.update(
            {
                "status": "RGB exact",
                "status_class": "part",
                "evidence": alpha_gap,
            }
        )
    elif renderer["id"] == "desktop_raylib" and raylib_gap:
        status.update(
            {
                "status": "compile gap" if source_path in GENERATED_C_COMPILE_GAPS else "visual gap",
                "status_class": "part",
                "evidence": raylib_gap,
            }
        )
    elif renderer["id"] == "desktop_libdraw_c" and libdraw_gap:
        status.update(
            {
                "status": "compile gap" if source_path in GENERATED_C_COMPILE_GAPS else "visual gap",
                "status_class": "part",
                "evidence": libdraw_gap,
            }
        )
    elif renderer["id"] == "web_canvas_wasm" and web_canvas_gap:
        status.update(
            {
                "status": "compile gap" if source_path in GENERATED_C_COMPILE_GAPS else "visual gap",
                "status_class": "part",
                "evidence": web_canvas_gap,
            }
        )
    return status


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
        libdraw_gap = LIBDRAW_C_VISUAL_GAPS.get(r)
        raylib_gap = RAYLIB_C_VISUAL_GAPS.get(r)
        web_canvas_gap = WEB_CANVAS_C_VISUAL_GAPS.get(r)
        pipelines = {}
        for pipeline in PIPELINES:
            pipelines[pipeline["id"]] = {
                "status": "gated",
                "status_class": "ok",
                "evidence": "conformance-matrix-check",
            }
        alpha_gap = KRB_ALPHA_BYTE_GAPS.get(r)
        renderer_matrix = {
            renderer["id"]: source_renderer_status(renderer, r, alpha_gap, raylib_gap, libdraw_gap, web_canvas_gap)
            for renderer in SOURCE_RENDERERS
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
                    "libdraw_c": {
                        "status": (
                            "compile gap" if r in GENERATED_C_COMPILE_GAPS
                            else "visual gap" if libdraw_gap
                            else "captured"
                        ),
                        "status_class": "part" if libdraw_gap else "ok",
                        "evidence": libdraw_gap or "make libdraw-matrix-check",
                    },
                },
                "renderer_matrix": renderer_matrix,
            }
        )
    return cases


def matrix() -> dict:
    cases = source_cases()
    comparisons = visual_comparisons(len(cases))
    widget_cases: dict[str, list[str]] = {widget: [] for widget in sorted(WIDGETS)}
    semantic_count = 0
    for case in cases:
        semantic_count += int(case["semantic_parity"])
        for widget in case["widgets"]:
            widget_cases.setdefault(widget, []).append(case["path"])
    widget_rows = [
        {
            "id": widget,
            "status": "covered" if paths else "missing",
            "status_class": "ok" if paths else "no",
            "source_count": len(paths),
            "sources": paths,
            "semantic_sources": [
                case["path"]
                for case in cases
                if case["semantic_parity"] and widget in case["widgets"]
            ],
        }
        for widget, paths in sorted(widget_cases.items())
    ]
    covered_widgets = sum(1 for row in widget_rows if row["source_count"])
    renderer_cells = [
        cell
        for case in cases
        for cell in case["renderer_matrix"].values()
    ]
    return {
        "schema": 2,
        "source": "scripts/conformance-matrix.py",
        "summary": {
            "source_cases": len(cases),
            "examples": sum(1 for case in cases if case["type"] == "example"),
            "parity_fixtures": sum(1 for case in cases if case["type"] == "parity fixture"),
            "pipeline_cells": len(cases) * len(PIPELINES),
            "semantic_parity_cases": semantic_count,
            "krb_rgb_visual_cases": len(cases),
            "krb_alpha_byte_exact_cases": len(cases) - len(KRB_ALPHA_BYTE_GAPS),
            "raylib_c_visual_cases": len(cases) - len(RAYLIB_C_VISUAL_GAPS),
            "raylib_c_visual_gaps": len(RAYLIB_C_VISUAL_GAPS),
            "web_canvas_c_visual_cases": len(cases) - len(WEB_CANVAS_C_VISUAL_GAPS),
            "web_canvas_c_visual_gaps": len(WEB_CANVAS_C_VISUAL_GAPS),
            "libdraw_c_visual_cases": len(cases) - len(LIBDRAW_C_VISUAL_GAPS),
            "libdraw_c_visual_gaps": len(LIBDRAW_C_VISUAL_GAPS),
            "visual_comparison_rows": len(comparisons),
            "visual_comparison_exact_rows": sum(1 for row in comparisons if row["status_class"] == "ok"),
            "visual_comparison_pending_rows": sum(1 for row in comparisons if row["status_class"] == "part"),
            "renderer_source_cells": len(renderer_cells),
            "renderer_source_ok_cells": sum(1 for cell in renderer_cells if cell["status_class"] == "ok"),
            "renderer_source_partial_cells": sum(1 for cell in renderer_cells if cell["status_class"] == "part"),
            "widgets_declared": len(widget_rows),
            "widgets_detected": covered_widgets,
            "widgets_missing": len(widget_rows) - covered_widgets,
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
        "source_renderers": [
            {
                "id": item["id"],
                "label": item["label"],
                "scope": item["scope"],
            }
            for item in SOURCE_RENDERERS
        ],
        "visual_comparisons": comparisons,
        "renderer_checks": [
            {
                "id": item["id"],
                "label": item["label"],
                "command": " ".join(item["command"]),
                "scope": item["scope"],
            }
            for item in RENDERER_SMOKE_CHECKS
        ],
        "runtime_checks": [
            {
                "id": item["id"],
                "label": item["label"],
                "command": " ".join(item["command"]),
                "scope": item["scope"],
            }
            for item in RUNTIME_PARITY_CHECKS
        ],
        "downstream_checks": [
            {
                "id": item["id"],
                "label": item["label"],
                "command": " ".join(item["command"]),
                "scope": item["scope"],
            }
            for item in DOWNSTREAM_CHECKS
        ],
        "widget_coverage": widget_rows,
        "widget_counts": {
            row["id"]: row["source_count"]
            for row in widget_rows
            if row["source_count"]
        },
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


def verify_widget_coverage(data: dict) -> int:
    missing = [row["id"] for row in data["widget_coverage"] if not row["source_count"]]
    if missing:
        print("widget coverage missing .kry source cases:", file=sys.stderr)
        for widget in missing:
            print(f"  {widget}", file=sys.stderr)
        return 1
    print(f"widget coverage ok: {len(data['widget_coverage'])} widgets covered by .kry matrix sources")
    return 0


def emcc_path() -> str | None:
    found = shutil.which("emcc")
    if found:
        return found
    fallback = Path.home() / "emsdk" / "upstream" / "emscripten" / "emcc"
    if fallback.exists():
        return str(fallback)
    return None


def verify_krb_web_visuals(data: dict) -> int:
    emcc = emcc_path()
    node = shutil.which("node")
    if emcc is None:
        print("KRB web matrix skipped: emcc not found")
        return 0
    if node is None:
        print("KRB web matrix skipped: node not found")
        return 0
    tools = {
        "k2b": ROOT / "build/linux-x86_64/bin/k2b",
        "krb-run": ROOT / "build/linux-x86_64/bin/krb-run",
    }
    missing = [f"{name}: {path}" for name, path in tools.items() if not path.exists()]
    if missing:
        for item in missing:
            print(f"missing tool for KRB web matrix: {item}", file=sys.stderr)
        return 1

    failures = []
    with tempfile.TemporaryDirectory(prefix="kryon-krb-web-matrix.") as tmp:
        work = Path(tmp)
        runner = work / "krb-web-capture.js"
        compile_cmd = [
            emcc,
            "-Wall",
            "-Wextra",
            "-Os",
            "-Iinclude",
            "-DKRB_WEB_ONESHOT",
            "-sENVIRONMENT=node",
            "-sEXIT_RUNTIME=1",
            f"-sEXPORTED_FUNCTIONS={KRB_WEB_EXPORTED_FUNCTIONS}",
            "-sEXPORTED_RUNTIME_METHODS=FS",
            "-sALLOW_MEMORY_GROWTH=1",
            "--pre-js",
            "cmd/krb-web/node-capture.js",
            "-o",
            str(runner),
        ] + KRB_WEB_SRCS
        run = subprocess.run(
            compile_cmd,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if run.returncode != 0:
            print("KRB web matrix build failed:", file=sys.stderr)
            for line in run.stdout.strip().splitlines()[-30:]:
                print(f"  {line}", file=sys.stderr)
            return 1

        for case in data["cases"]:
            source = ROOT / case["path"]
            stem = case["path"].removesuffix(".kry")
            out_dir = work / "krb"
            krb = out_dir / f"{stem}.krb"
            native_png = work / f"{stem}.native.png"
            native_png.parent.mkdir(parents=True, exist_ok=True)

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
                [
                    str(tools["krb-run"]),
                    "--png",
                    str(native_png),
                    "--w",
                    "800",
                    "--h",
                    "600",
                    str(krb),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                failures.append((case["path"], "krb-run", (run.stderr or run.stdout).strip()))
                continue
            run = subprocess.run(
                [node, str(runner), str(krb)],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                failures.append((case["path"], "krb-web", run.stderr.decode("utf-8", "replace").strip()))
                continue
            nw, nh, native = png_rgba(native_png)
            if (nw, nh) != (800, 600):
                failures.append((case["path"], "native-size", f"{nw}x{nh}"))
                continue
            if len(run.stdout) != len(native):
                failures.append((case["path"], "size", f"web={len(run.stdout)} native={len(native)}"))
                continue
            if run.stdout != native:
                diff = sum(1 for i in range(0, len(native), 4) if run.stdout[i:i + 4] != native[i:i + 4])
                failures.append((case["path"], "pixels", f"{diff} pixels differ"))

    if failures:
        for path, phase, detail in failures:
            print(f"KRB web matrix failed for {path} during {phase}", file=sys.stderr)
            if detail:
                print(f"  {detail}", file=sys.stderr)
        return 1
    print(f"KRB web matrix ok: {len(data['cases'])} sources byte-identical to native kry_sw")
    return 0


def generated_c_sources(out_dir: Path) -> list[str]:
    sources = [
        str(path)
        for path in sorted(out_dir.rglob("*.c"))
        if path.name != "kryon_project.c"
    ]
    project = out_dir / "kryon_project.c"
    if project.exists():
        sources.append(str(project))
    return sources


def canvas_backend_sources() -> list[str]:
    sources = []
    for path in sorted((ROOT / "src").rglob("*.c")):
        r = rel(path)
        if "/ksync/" in f"/{r}/":
            continue
        if "/platform/plan9/" in f"/{r}/":
            continue
        if r in {
            "src/scene/physics_world.c",
            "src/scene/node_body2d.c",
            "src/scene/node_area2d.c",
            "src/scene/node_collision_shape2d.c",
        }:
            continue
        if path.name.startswith("libdraw_"):
            continue
        sources.append(str(path))
    return sources


def png_has_content(path: Path) -> bool:
    width, height, pixels = png_rgba(path)
    if width <= 0 or height <= 0:
        return False
    first = pixels[:4]
    for i in range(4, len(pixels), 4):
        if pixels[i:i + 4] != first:
            return True
    return False


def write_raylib_window_capture_script(path: Path) -> None:
    path.write_text(
        """#!/bin/sh
set -eu
out=$1
shift
"$@" &
pid=$!
cleanup() {
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
win=""
i=0
while [ "$i" -lt 50 ]; do
    for id in $(xdotool search --onlyvisible --name "Kryon generated C capture" 2>/dev/null || true); do
        geom=$(xdotool getwindowgeometry --shell "$id" 2>/dev/null || true)
        w=$(printf '%s\n' "$geom" | awk -F= '/^WIDTH/{print $2}')
        h=$(printf '%s\n' "$geom" | awk -F= '/^HEIGHT/{print $2}')
        if [ -n "$w" ] && [ -n "$h" ] && [ "$w" -gt 0 ] && [ "$h" -gt 0 ]; then
            win=$id
            break
        fi
    done
    [ -n "$win" ] && break
    sleep 0.1
    i=$((i + 1))
done
if [ -z "$win" ]; then
    echo "raylib-window-capture: no generated-C window found" >&2
    exit 1
fi
sleep 1.5
tmp="${out}.capture.png"
import -window "$win" "$tmp"
convert "$tmp" PNG32:"$out"
rm -f "$tmp"
test -s "$out"
""",
        encoding="utf-8",
    )
    path.chmod(0o755)


def verify_web_canvas_c_visuals(data: dict, args: argparse.Namespace) -> int:
    emcc = emcc_path()
    node = shutil.which("node")
    if emcc is None:
        print("web Canvas generated-C matrix skipped: emcc not found")
        return 0
    if node is None:
        print("web Canvas generated-C matrix skipped: node not found")
        return 0

    k2c = Path(args.k2c)
    if not k2c.exists():
        print(f"missing tool for web Canvas generated-C matrix: {k2c}", file=sys.stderr)
        return 1

    assets = ROOT / "build" / "linux-x86_64" / "embedded_asset_data.c"
    if not assets.exists():
        print(f"missing embedded assets for web Canvas generated-C matrix: {assets}", file=sys.stderr)
        return 1

    failures = []
    observed_gaps = set()
    backend_sources = canvas_backend_sources()
    with tempfile.TemporaryDirectory(prefix="kryon-web-canvas-c-matrix.") as tmp:
        work = Path(tmp)
        for case in data["cases"]:
            source = ROOT / case["path"]
            out_dir = work / "gen" / case["id"]
            js_path = work / "js" / f"{case['id']}.js"
            png_path = work / "png" / f"{case['id']}.png"
            out_dir.mkdir(parents=True, exist_ok=True)
            js_path.parent.mkdir(parents=True, exist_ok=True)
            png_path.parent.mkdir(parents=True, exist_ok=True)

            run = subprocess.run(
                [
                    str(k2c),
                    "--no-main",
                    "--root",
                    str(ROOT),
                    "-o",
                    str(out_dir),
                    str(source),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "k2c", (run.stderr or run.stdout).strip()))
                continue

            compile_cmd = [
                emcc,
                "-Iinclude",
                "-I",
                str(out_dir),
                "-Iexamples",
                "-DKRYON_WITH_PHYSICS=0",
                "-O1",
                "-sASYNCIFY",
                "-sENVIRONMENT=node",
                "-sINITIAL_MEMORY=128MB",
                "-sALLOW_MEMORY_GROWTH=1",
                "-sEXIT_RUNTIME=0",
                "-sEXPORTED_RUNTIME_METHODS=FS",
                "--pre-js",
                "tests/canvas_generated_c_capture_run.js",
                "-o",
                str(js_path),
                "tests/generated_c_capture_main.c",
            ] + generated_c_sources(out_dir) + backend_sources + [str(assets)]
            run = subprocess.run(
                compile_cmd,
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if run.returncode != 0:
                if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "compile", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue

            env = os.environ.copy()
            env["KRYON_CANVAS_CAPTURE_OUT"] = str(png_path)
            run = subprocess.run(
                [
                    node,
                    str(js_path),
                    "--png",
                    "/canvas-capture.png",
                    "--w",
                    "480",
                    "--h",
                    "640",
                    "--frames",
                    "4",
                    "--source",
                    case["path"],
                ],
                cwd=ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=45,
            )
            if run.returncode != 0:
                if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "capture", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue
            if not png_path.exists() or png_path.stat().st_size == 0:
                if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", "capture did not produce a PNG"))
                continue
            try:
                width, height, _pixels = png_rgba(png_path)
                if width <= 0 or height <= 0:
                    failures.append((case["path"], "size", f"{width}x{height}"))
                    continue
                if not png_has_content(png_path):
                    if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                        observed_gaps.add(case["path"])
                    else:
                        detail = "all pixels are identical"
                        tail = "\n".join(run.stdout.strip().splitlines()[-20:])
                        if tail:
                            detail += "\n" + tail
                        failures.append((case["path"], "blank", detail))
            except ValueError as exc:
                if case["path"] in WEB_CANVAS_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", str(exc)))

    case_paths = {case["path"] for case in data["cases"]}
    recovered = sorted((set(WEB_CANVAS_C_VISUAL_GAPS) & case_paths) - observed_gaps)
    if recovered:
        for path in recovered:
            failures.append((path, "known-gap", "listed web Canvas C visual gap now captures; update the matrix gap list"))

    if failures:
        for path, phase, detail in failures:
            print(f"web Canvas generated-C matrix failed for {path} during {phase}", file=sys.stderr)
            if detail:
                print(f"  {detail}", file=sys.stderr)
        return 1
    captured = len(data["cases"]) - len(observed_gaps)
    print(
        f"web Canvas generated-C matrix ok: {captured} sources captured through wasm Canvas2D, "
        f"{len(observed_gaps)} known generated-C gaps"
    )
    return 0


def verify_raylib_c_visuals(data: dict, args: argparse.Namespace) -> int:
    xvfb = shutil.which("xvfb-run")
    if xvfb is None and not os.environ.get("DISPLAY"):
        print("raylib generated-C matrix skipped: no DISPLAY and xvfb-run not found")
        return 0

    k2c = Path(args.k2c)
    if not k2c.exists():
        print(f"missing tool for raylib generated-C matrix: {k2c}", file=sys.stderr)
        return 1

    cc = args.cc
    cppflags = shlex.split(args.cppflags)
    cflags = shlex.split(args.cflags)
    ldinputs = shlex.split(args.ldinputs)
    if not ldinputs:
        print("raylib generated-C matrix requires --ldinputs from Makefile", file=sys.stderr)
        return 1

    failures = []
    observed_gaps = set()
    window_capture = shutil.which("import") is not None and shutil.which("xdotool") is not None
    with tempfile.TemporaryDirectory(prefix="kryon-raylib-c-matrix.") as tmp:
        work = Path(tmp)
        capture_script = work / "raylib-window-capture.sh"
        if window_capture:
            write_raylib_window_capture_script(capture_script)
        for case in data["cases"]:
            source = ROOT / case["path"]
            out_dir = work / "gen" / case["id"]
            bin_path = work / "bin" / case["id"]
            png_path = work / "png" / f"{case['id']}.png"
            internal_png_path = work / "png" / f"{case['id']}.internal.png"
            out_dir.mkdir(parents=True, exist_ok=True)
            bin_path.parent.mkdir(parents=True, exist_ok=True)
            png_path.parent.mkdir(parents=True, exist_ok=True)

            run = subprocess.run(
                [
                    str(k2c),
                    "--no-main",
                    "--root",
                    str(ROOT),
                    "-o",
                    str(out_dir),
                    str(source),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                if case["path"] in RAYLIB_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "k2c", (run.stderr or run.stdout).strip()))
                continue

            compile_cmd = (
                [cc]
                + cppflags
                + cflags
                + ["-I", str(out_dir), "-Iexamples", "tests/generated_c_capture_main.c"]
                + generated_c_sources(out_dir)
                + ldinputs
                + ["-o", str(bin_path)]
            )
            run = subprocess.run(
                compile_cmd,
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if run.returncode != 0:
                if case["path"] in RAYLIB_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "compile", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue

            env = os.environ.copy()
            env["KRYON_SHOT_ARM"] = "1"
            app_png_path = internal_png_path if window_capture else png_path
            capture = [
                str(bin_path),
                "--png",
                str(app_png_path),
                "--w",
                "480",
                "--h",
                "640",
                "--frames",
                "4",
                "--source",
                case["path"],
            ]
            if window_capture:
                capture += ["--hold-before-capture-ms", "8000"]
                if xvfb is not None:
                    run_cmd = [xvfb, "-a", "env", "KRYON_SHOT_ARM=1", "sh", str(capture_script), str(png_path)] + capture
                    run_env = os.environ.copy()
                else:
                    run_cmd = ["sh", str(capture_script), str(png_path)] + capture
                    run_env = env
            elif xvfb is not None:
                run_cmd = [xvfb, "-a", "env", "KRYON_SHOT_ARM=1"] + capture
                run_env = os.environ.copy()
            else:
                run_cmd = capture
                run_env = env
            run = subprocess.run(
                run_cmd,
                cwd=ROOT,
                env=run_env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=30,
            )
            if run.returncode != 0:
                if case["path"] in RAYLIB_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "capture", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue
            if not png_path.exists() or png_path.stat().st_size == 0:
                if case["path"] in RAYLIB_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", "capture did not produce a PNG"))
                continue
            try:
                width, height, _pixels = png_rgba(png_path)
                if width <= 0 or height <= 0:
                    failures.append((case["path"], "size", f"{width}x{height}"))
                    continue
                if not png_has_content(png_path):
                    if case["path"] in RAYLIB_C_VISUAL_GAPS:
                        observed_gaps.add(case["path"])
                    else:
                        failures.append((case["path"], "blank", "all pixels are identical"))
            except ValueError as exc:
                if case["path"] in RAYLIB_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", str(exc)))

    case_paths = {case["path"] for case in data["cases"]}
    recovered = sorted((set(RAYLIB_C_VISUAL_GAPS) & case_paths) - observed_gaps)
    if recovered:
        for path in recovered:
            failures.append((path, "known-gap", "listed raylib C visual gap now captures; update the matrix gap list"))

    if failures:
        for path, phase, detail in failures:
            print(f"raylib generated-C matrix failed for {path} during {phase}", file=sys.stderr)
            if detail:
                print(f"  {detail}", file=sys.stderr)
        return 1
    captured = len(data["cases"]) - len(observed_gaps)
    print(
        f"raylib generated-C matrix ok: {captured} sources captured through raylib, "
        f"{len(observed_gaps)} known generated-C gaps"
    )
    return 0


def verify_libdraw_c_visuals(data: dict, args: argparse.Namespace) -> int:
    plan9 = Path(args.plan9port_dir)
    if not (plan9 / "bin" / "devdraw").exists() or not (plan9 / "include").is_dir():
        print(f"libdraw generated-C matrix skipped: plan9port not found at {plan9}")
        return 0
    xvfb = shutil.which("xvfb-run")
    if xvfb is None and not os.environ.get("DISPLAY"):
        print("libdraw generated-C matrix skipped: no DISPLAY and xvfb-run not found")
        return 0

    k2c = ROOT / "build" / "linux-x86_64" / "bin" / "k2c"
    if not k2c.exists():
        print(f"missing tool for libdraw generated-C matrix: {k2c}", file=sys.stderr)
        return 1

    cc = args.cc
    cppflags = shlex.split(args.cppflags)
    cflags = shlex.split(args.cflags)
    ldinputs = shlex.split(args.ldinputs)
    if not ldinputs:
        print("libdraw generated-C matrix requires --ldinputs from Makefile", file=sys.stderr)
        return 1

    failures = []
    observed_gaps = set()
    with tempfile.TemporaryDirectory(prefix="kryon-libdraw-c-matrix.") as tmp:
        work = Path(tmp)
        for case in data["cases"]:
            source = ROOT / case["path"]
            out_dir = work / "gen" / case["id"]
            bin_path = work / "bin" / case["id"]
            png_path = work / "png" / f"{case['id']}.png"
            out_dir.mkdir(parents=True, exist_ok=True)
            bin_path.parent.mkdir(parents=True, exist_ok=True)
            png_path.parent.mkdir(parents=True, exist_ok=True)

            run = subprocess.run(
                [
                    str(k2c),
                    "--no-main",
                    "--root",
                    str(ROOT),
                    "-o",
                    str(out_dir),
                    str(source),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if run.returncode != 0:
                if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "k2c", (run.stderr or run.stdout).strip()))
                continue

            compile_cmd = (
                [cc]
                + cppflags
                + cflags
                + ["-I", str(out_dir), "-Iexamples", "tests/generated_c_capture_main.c"]
                + generated_c_sources(out_dir)
                + ldinputs
                + ["-o", str(bin_path)]
            )
            run = subprocess.run(
                compile_cmd,
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if run.returncode != 0:
                if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "compile", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue

            env = os.environ.copy()
            env["PLAN9"] = str(plan9)
            env["DEVDRAW"] = str(plan9 / "bin" / "devdraw")
            env["PATH"] = f"{plan9 / 'bin'}:{env.get('PATH', '')}"
            capture = [
                str(bin_path),
                "--png",
                str(png_path),
                "--w",
                "480",
                "--h",
                "640",
                "--frames",
                "4",
                "--source",
                case["path"],
            ]
            if xvfb is not None:
                run_cmd = [xvfb, "-a", "env"] + [f"{key}={value}" for key, value in {
                    "PLAN9": env["PLAN9"],
                    "DEVDRAW": env["DEVDRAW"],
                    "PATH": env["PATH"],
                }.items()] + capture
                run_env = os.environ.copy()
            else:
                run_cmd = capture
                run_env = env
            run = subprocess.run(
                run_cmd,
                cwd=ROOT,
                env=run_env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=30,
            )
            if run.returncode != 0:
                if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "capture", "\n".join(run.stdout.strip().splitlines()[-20:])))
                continue
            if not png_path.exists() or png_path.stat().st_size == 0:
                if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", "capture did not produce a PNG"))
                continue
            try:
                width, height, _pixels = png_rgba(png_path)
                if width <= 0 or height <= 0:
                    failures.append((case["path"], "size", f"{width}x{height}"))
                    continue
                if not png_has_content(png_path):
                    if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                        observed_gaps.add(case["path"])
                    else:
                        failures.append((case["path"], "blank", "all pixels are identical"))
            except ValueError as exc:
                if case["path"] in LIBDRAW_C_VISUAL_GAPS:
                    observed_gaps.add(case["path"])
                else:
                    failures.append((case["path"], "png", str(exc)))

    case_paths = {case["path"] for case in data["cases"]}
    recovered = sorted((set(LIBDRAW_C_VISUAL_GAPS) & case_paths) - observed_gaps)
    if recovered:
        for path in recovered:
            failures.append((path, "known-gap", "listed libdraw C visual gap now captures; update the matrix gap list"))

    if failures:
        for path, phase, detail in failures:
            print(f"libdraw generated-C matrix failed for {path} during {phase}", file=sys.stderr)
            if detail:
                print(f"  {detail}", file=sys.stderr)
        return 1
    captured = len(data["cases"]) - len(observed_gaps)
    print(
        f"libdraw generated-C matrix ok: {captured} sources captured through libdraw, "
        f"{len(observed_gaps)} known generated-C gaps"
    )
    return 0


def resolve_command(command: list[str]) -> list[str]:
    out = list(command)
    for i, arg in enumerate(out):
        if arg == ".":
            out[i] = str(ROOT)
        elif arg.startswith("../"):
            out[i] = str((ROOT / arg).resolve())
    return out


def run_checks(checks: list[dict], label: str) -> int:
    failures = []
    for check in checks:
        optional_dir = check.get("optional_dir")
        if optional_dir is not None and not (ROOT / optional_dir).resolve().is_dir():
            print(f"{label} skipped: {check['id']} ({optional_dir} not found)")
            continue
        command = resolve_command(check["command"])
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
            print(f"{label} ok: {check['id']}")
    if failures:
        for check_id, tail in failures:
            print(f"{label} failed: {check_id}", file=sys.stderr)
            for line in tail:
                print(f"  {line}", file=sys.stderr)
        return 1
    return 0


def verify_renderer_smokes() -> int:
    return run_checks(RENDERER_SMOKE_CHECKS, "renderer smoke")


def verify_runtime_parity() -> int:
    return run_checks(RUNTIME_PARITY_CHECKS, "runtime parity")


def verify_downstream() -> int:
    return run_checks(DOWNSTREAM_CHECKS, "downstream")


def verify_visual_comparison_matrix(data: dict) -> int:
    rows = data.get("visual_comparisons", [])
    failures = []
    required = {
        "krb-native-vs-sdl",
        "krb-native-vs-web-canvas",
        "raylib-c-vs-krb-native",
        "libdraw-c-vs-krb-native",
        "web-canvas-c-vs-krb-native",
        "web-raylib-vs-native-raylib",
    }
    ids = {row.get("id") for row in rows}
    missing = sorted(required - ids)
    extra = sorted(ids - required)
    if missing:
        failures.append(("ids", "missing visual comparison rows: " + ", ".join(missing)))
    if extra:
        failures.append(("ids", "unexpected visual comparison rows: " + ", ".join(extra)))

    source_count = data["summary"]["source_cases"]
    for row in rows:
        row_id = row.get("id", "<unknown>")
        compared = row.get("compared", -1)
        total = row.get("source_cases", -1)
        status_class = row.get("status_class")
        status = row.get("status", "")
        mode = row.get("mode", "")
        evidence = row.get("evidence", [])

        if total != source_count:
            failures.append((row_id, f"source_cases={total}, want {source_count}"))
        if compared < 0 or compared > source_count:
            failures.append((row_id, f"compared={compared}, outside 0..{source_count}"))
        if status_class == "ok" and compared != source_count:
            failures.append((row_id, "exact comparison rows must cover every source"))
        if status_class == "ok" and ("pending" in status or "pending" in mode):
            failures.append((row_id, "pending comparison cannot be marked ok"))
        if status_class != "ok" and ("exact" in status or "exact" in mode):
            failures.append((row_id, "exact comparison must be marked ok"))
        if not evidence:
            failures.append((row_id, "missing evidence"))

    exact = sum(1 for row in rows if row.get("status_class") == "ok")
    pending = sum(1 for row in rows if row.get("status_class") == "part")
    summary = data["summary"]
    if summary.get("visual_comparison_rows") != len(rows):
        failures.append(("summary", "visual_comparison_rows does not match table length"))
    if summary.get("visual_comparison_exact_rows") != exact:
        failures.append(("summary", "visual_comparison_exact_rows does not match table"))
    if summary.get("visual_comparison_pending_rows") != pending:
        failures.append(("summary", "visual_comparison_pending_rows does not match table"))

    if failures:
        for row_id, detail in failures:
            print(f"visual comparison matrix failed for {row_id}: {detail}", file=sys.stderr)
        return 1
    print(f"visual comparison matrix ok: {exact} exact rows, {pending} pending rows")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated website JSON is current")
    parser.add_argument("--verify-pipelines", action="store_true", help="run all listed sources through k2ir/k2c/k2g/k2b")
    parser.add_argument("--verify-krb-visuals", action="store_true", help="compare KRB headless PNGs against SDL readback PNGs")
    parser.add_argument("--verify-widget-coverage", action="store_true", help="verify every declared matrix widget appears in a .kry source")
    parser.add_argument("--verify-krb-web-visuals", action="store_true", help="compare KRB web wasm capture against native kry_sw for every source")
    parser.add_argument("--verify-web-canvas-c-visuals", action="store_true", help="compile generated C for every source to wasm Canvas2D and capture one PNG")
    parser.add_argument("--verify-raylib-c-visuals", action="store_true", help="compile generated C for every source against raylib and capture one PNG")
    parser.add_argument("--verify-libdraw-c-visuals", action="store_true", help="compile generated C for every source against libdraw and capture one PNG")
    parser.add_argument("--verify-renderer-smokes", action="store_true", help="run non-per-source renderer smoke gates")
    parser.add_argument("--verify-runtime-parity", action="store_true", help="run runtime-level parity gates")
    parser.add_argument("--verify-downstream", action="store_true", help="run downstream consumer gates when available")
    parser.add_argument("--verify-visual-comparison-matrix", action="store_true", help="verify visual comparison matrix status accounting")
    parser.add_argument("--cc", default="cc", help=argparse.SUPPRESS)
    parser.add_argument("--k2c", default=str(ROOT / "build" / "linux-x86_64" / "bin" / "k2c"), help=argparse.SUPPRESS)
    parser.add_argument("--cppflags", default="", help=argparse.SUPPRESS)
    parser.add_argument("--cflags", default="", help=argparse.SUPPRESS)
    parser.add_argument("--ldinputs", default="", help=argparse.SUPPRESS)
    parser.add_argument("--plan9port-dir", default=os.environ.get("PLAN9PORT_DIR", "/mnt/storage/Projects/plan9port"), help=argparse.SUPPRESS)
    args = parser.parse_args()

    data = matrix()
    rendered = encode(data)
    if args.check:
        rc = check_output(rendered)
        if rc:
            return rc
    elif not any((args.verify_pipelines, args.verify_krb_visuals, args.verify_widget_coverage, args.verify_krb_web_visuals, args.verify_web_canvas_c_visuals, args.verify_raylib_c_visuals, args.verify_libdraw_c_visuals, args.verify_renderer_smokes, args.verify_runtime_parity, args.verify_downstream, args.verify_visual_comparison_matrix)):
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
    if args.verify_widget_coverage:
        rc = verify_widget_coverage(data)
        if rc:
            return rc
    if args.verify_krb_web_visuals:
        rc = verify_krb_web_visuals(data)
        if rc:
            return rc
    if args.verify_web_canvas_c_visuals:
        rc = verify_web_canvas_c_visuals(data, args)
        if rc:
            return rc
    if args.verify_raylib_c_visuals:
        rc = verify_raylib_c_visuals(data, args)
        if rc:
            return rc
    if args.verify_libdraw_c_visuals:
        rc = verify_libdraw_c_visuals(data, args)
        if rc:
            return rc
    if args.verify_renderer_smokes:
        rc = verify_renderer_smokes()
        if rc:
            return rc
    if args.verify_runtime_parity:
        rc = verify_runtime_parity()
        if rc:
            return rc
    if args.verify_downstream:
        rc = verify_downstream()
        if rc:
            return rc
    if args.verify_visual_comparison_matrix:
        rc = verify_visual_comparison_matrix(data)
        if rc:
            return rc
    return 0


if __name__ == "__main__":
    sys.exit(main())
