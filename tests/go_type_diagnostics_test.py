#!/usr/bin/env python3
"""Go lowering must not erase unsupported types or executable statements."""
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
cases = {
    "parameter": ("Check :: (value: Missing) {}", "unsupported or unresolved Go type"),
    "return": ("Check :: () -> Missing {}", "unsupported or unresolved Go type"),
    "local": ("Check :: () {\nvalue: Missing\n}", "unsupported or unresolved Go type"),
    "field": ("Value :: struct {\nitem: Missing\n}", "unsupported or unresolved Go type"),
    "array": ("Value :: struct {\nitems: [4] Missing\n}", "unsupported or unresolved Go type"),
    "slice": ("Value :: struct {\nitems: [] Missing\n}", "unsupported or unresolved Go type"),
    "pointer": ("Check :: (value: Missing*) {}", "unsupported or unresolved Go type"),
    "host": ('Check :: (value: Missing) #extern "host.Check"', "unsupported or unresolved Go type"),
    "state": ("state {\nvalue: Missing\n}", "unsupported or unresolved Go type"),
    "typedef": ("Callback :: int (*)(int) #type", "C typedef has no portable Go representation"),
    "raw": ("Check :: () {\nc puts(\"side effect\");\n}", "unsupported raw statement"),
}
with tempfile.TemporaryDirectory(prefix="kryon-go-type-diagnostics-") as directory:
    work = Path(directory)
    for name, (source, diagnostic) in cases.items():
        path = work / f"{name}.kry"
        path.write_text(source + "\n")
        output = work / name
        output.mkdir()
        result = subprocess.run([str(compiler), "--no-main", "--root", str(work),
                                 "-o", str(output), str(path)], capture_output=True, text=True)
        assert result.returncode != 0, (name, "unsupported source accepted")
        assert diagnostic in result.stderr, (name, result.stderr)
        assert f"{name}.kry:" in result.stderr, (name, "missing source location", result.stderr)
        assert not list(output.glob("*.go")), (name, "published partially lowered output")
print("Go unsupported-type and statement diagnostics pass")
