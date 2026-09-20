#!/usr/bin/env python3
"""Verify KSS host/parser call sites are classified in the P4-01 ledger."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEDGER = ROOT / "plan" / "KSS_HOST_SERVICE_LEDGER.md"

CALLSITE = re.compile(
    r"\b(kss_parse_string|kss_parse_trace_string|kss_parse_with_|kss_format_string|KssFormat|"
    r"ParseStyleSheet|ParseStyleVariants|RegisterStylePackSource|"
    r"ResolveStyle\(|parseWebStyleSheet|resolveWebStyle|traceWebStyle|"
    r"webStyleSheetToCSS)\b"
)

SCAN_ROOTS = ("src", "include", "cmd", "go/kryon", "scripts", "web")
SKIP_SUFFIXES = ("_test.go",)
SKIP_PATHS = {
    "go/kryon/style_builtins.go",
    "scripts/kss-host-ledger-check.py",
}

EXPECTED = {
    "cmd/k2c/k2c_project.c",
    "cmd/k2cpp/k2cpp_project.c",
    "cmd/k2go/k2go_lower.c",
    "cmd/kssfmt/main.c",
    "go/kryon/style_pack.go",
    "go/kryon/style_parse.go",
    "include/ui_style_sheet.h",
    "scripts/generate-go-style-builtins.py",
    "src/ui/kss_parser.kry",
    "src/ui/style_pack_source.kry",
    "src/ui/style_sheet.kry",
    "web/kryon-runtime.d.ts",
}

OWNER_ROWS = {
    "runtime/kss_parser.kry",
    "runtime/kss_formatter.kry",
    "runtime/style_sheet.kry",
}


def git_files():
    files = set()
    for extra in ([], ["--others", "--exclude-standard"]):
        out = subprocess.run(
            ["git", "-C", str(ROOT), "ls-files", *extra, *SCAN_ROOTS],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        files.update(line for line in out.splitlines() if line)
    return sorted(files)


def should_skip(path):
    if path in SKIP_PATHS:
        return True
    if path.startswith("web/") and path.endswith(".js"):
        return True
    if path.startswith("go/kryon/") and path.endswith(SKIP_SUFFIXES):
        return True
    return False


def main():
    ledger = LEDGER.read_text(errors="replace")
    found = set()
    for path in git_files():
        if should_skip(path):
            continue
        full = ROOT / path
        if not full.is_file():
            continue
        if CALLSITE.search(full.read_text(errors="replace")):
            found.add(path)

    failures = []
    missing_scan = sorted(EXPECTED - found)
    unexpected_scan = sorted(found - EXPECTED)
    if missing_scan:
        failures.append("expected call-site files without current hits: " + ", ".join(missing_scan))
    if unexpected_scan:
        failures.append("unclassified KSS call-site files: " + ", ".join(unexpected_scan))

    for path in sorted(EXPECTED | OWNER_ROWS):
        if f"`{path}`" not in ledger:
            failures.append(f"missing ledger row for {path}")

    required_classes = [
        "generated owner",
        "host service",
        "tool host service",
        "compiler-emitted host glue",
        "generated-data tool",
        "future web reference",
    ]
    for text in required_classes:
        if text not in ledger:
            failures.append(f"missing classification {text!r}")

    if failures:
        for failure in failures:
            print(f"kss-host-ledger-check: {failure}", file=sys.stderr)
        return 1
    print(f"kss-host-ledger-check: ok ({len(found)} host/tool call-site files, 3 generated owners)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
