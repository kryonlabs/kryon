#!/usr/bin/env python3
"""Invalid record constructors must fail in the shared strict checker."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bin_dir = Path(sys.argv[1]).resolve()
cases = {
    "unknown": ("(Props){.missing=1}", "unknown initializer field"),
    "duplicate": ("(Props){.count=1, .count=2}", "duplicate initializer field"),
    "mixed": ("(Props){1, .active=true}", "cannot mix named and positional"),
    "extra": ("(Props){1, true, 3}", "too many positional record fields"),
    "wrong_type": ('(Props){.count="bad"}', "initializer field type mismatch"),
    "wrong_nested": ("(Outer){.props=1}", "initializer field type mismatch"),
}
with tempfile.TemporaryDirectory(prefix="kryon-record-initializers-") as directory:
    work = Path(directory)
    for name, (value, diagnostic) in cases.items():
        source = work / f"{name}.kry"
        result_type = "Outer" if name == "wrong_nested" else "Props"
        source.write_text(
            'Props :: struct {\n    count: i32\n    active: bool\n}\n'
            'Outer :: struct {\n    props: Props\n}\n'
            f'Check :: () -> {result_type} #export {{\n    return {value}\n}}\n')
        for target in ("k2c", "k2cpp", "k2go", "k2js"):
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                 "-o", str(work / target / name), str(source)],
                cwd=root, capture_output=True, text=True)
            assert result.returncode != 0, (target, name, "invalid initializer accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)
print("Record initializer diagnostics agree in C, C++, Go, and JavaScript")
