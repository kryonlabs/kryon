#!/usr/bin/env python3
"""Portable string failures must be diagnostics, not invalid generated code."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bin_dir = Path(sys.argv[1]).resolve()
cases = {
    "addition": ('return "a" + "b"', "string operation is not supported", "string"),
    "ordering": ('return "a" < "b"', "string operation is not supported", "bool"),
    "cast": ('return (i32)"a"', "string casts require", "i32"),
    "postfix": ('value: string = "a"\n    value++\n    return value', "numeric value", "string"),
    "compound": ('value: string = "a"\n    value += "b"\n    return value', "string compound assignment", "string"),
    "escape": (r'return "\q"', "unknown string escape", "string"),
    "utf8": (r'return "\xFF"', "not valid UTF-8", "string"),
    "surrogate": (r'return "\uD800"', "Unicode scalar value", "string"),
}
with tempfile.TemporaryDirectory(prefix="kryon-string-diagnostics-") as directory:
    work = Path(directory)
    for name, (body, diagnostic, result_type) in cases.items():
        source = work / f"{name}.kry"
        source.write_text(f'Check :: () -> {result_type} #export {{\n    {body}\n}}\n')
        for target in ("k2c", "k2cpp", "k2go", "k2js"):
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--root", str(work),
                 "-o", str(work / target / name), str(source)],
                cwd=root, capture_output=True, text=True)
            assert result.returncode != 0, (target, name, "invalid source accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)
print("String diagnostics agree in C, C++, Go, and JavaScript")
