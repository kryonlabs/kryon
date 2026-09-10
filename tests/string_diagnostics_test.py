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
    "borrowed_null": ('value: const char* = "a\\0b"\n return true', "cannot contain a null byte", "bool"),
    "borrowed_hex_null": ('value: const char* = "a\\x00b"\n return true', "cannot contain a null byte", "bool"),
    "borrowed_conversion": ('text: string = "label"\n value: const char* = text\n return true', "borrowed string requires", "bool"),
    "borrowed_comparison": ('text: string = "label"\n value: const char* = "label"\n return value == text', "borrowed string requires", "bool"),
    "borrowed_ordering": ('value: const char* = "label"\n return value < "other"', "string operation is not supported", "bool"),
    "borrowed_cast": ('value: const char* = "label"\n return (i32)value', "string casts require", "i32"),
    "borrowed_return_null": (r'return "a\u0000b"', "cannot contain a null byte", "const char*"),
    "borrowed_return_view": ('value: string = "label"\n return value', "borrowed string requires", "const char*"),
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
