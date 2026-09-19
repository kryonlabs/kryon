#!/usr/bin/env python3
"""Portable aggregate diagnostics and array read/write bounds agree natively."""
from pathlib import Path
import os
import resource
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bin_dir = Path(sys.argv[1]).resolve()
targets = ("k2c", "k2cpp", "k2go")
cases = {
    "unknown": ("Box :: struct {\nvalue: Missing\n}", "unknown stored type"),
    "slice": ("Box :: struct {\nvalue: []i32\n}", "slice descriptors cannot be stored in aggregates"),
    "zero": ("Box :: struct {\nvalue: [0]i32\n}", "positive capacity"),
    "large": ("Box :: struct {\nvalue: [99999999999999999999]i32\n}", "malformed fixed array type"),
    "nested": ("Box :: struct {\nvalue: [2][3]i32\n}", "nested fixed arrays"),
    "void": ("Box :: struct {\nvalue: [2]void\n}", "void type"),
    "recursive": ("Box :: struct {\nvalue: Box\n}", "recursive value layout"),
    "mutual": ("Box :: struct {\nvalue: Other\n}\nOther :: struct {\nvalue: Box\n}", "recursive value layout"),
    "array_recursive": ("Box :: struct {\nvalue: [2]Box\n}", "recursive value layout"),
    "slot_array": ("Content :: () #slot\nBox :: struct {\nvalue: [2]Content\n}", "slot values cannot be stored"),
    "float_index": ("Box :: struct {\nvalue: [2]i32\n}\nRead :: (box: Box) -> i32 {\nreturn box.value[0.5]\n}", "array index requires an integer"),
    "float_string_index": ('Read :: (text: string) -> u8 {\nreturn text[0.5]\n}', "string index requires an integer"),
    "slice_field": ("Box :: struct {\nitems: []i32\n}", "slice descriptors cannot be stored in aggregates"),
    "slice_return": ("Read :: () -> []i32 {}", "return on every path"),
    "unknown_state": ("state {\nitem: Missing\n}", "unknown stored type"),
    "slot_pointer_state": ("Content :: () #slot\nstate {\nitem: Content*\n}", "slot values cannot be stored"),
}

def run(args, **kwargs):
    result = subprocess.run(args, capture_output=True, text=True, **kwargs)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix="kryon-aggregate-types-") as directory:
    work = Path(directory)
    for name, (source, diagnostic) in cases.items():
        path = work / f"{name}.kry"
        path.write_text(source + "\n")
        for target in targets:
            result = subprocess.run([str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                                     "-o", str(work / target / name), str(path)], capture_output=True, text=True)
            assert result.returncode != 0, (target, name, "invalid type accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)
            assert f"{name}.kry:" in result.stderr, (target, name, "missing source location")

    source = work / "check.kry"
    source.write_text('''#module "check"
CAPACITY :: 2
Values :: struct {
    data: [2] i32
    named: [CAPACITY]i32
}
Read :: (index: i32) -> i32 #export {
    value: Values
    value.data[0] = 7
    return value.data[index]
}
Write :: (index: i32) -> i32 #export {
    value: Values
    value.data[index] = 7
    return value.data[0]
}
ReadNamed :: (index: i32) -> i32 #export {
    value: Values
    return value.named[index]
}
WriteNamed :: (index: i32) -> i32 #export {
    value: Values
    value.named[index] = 7
    return value.named[0]
}
''')
    for target in targets:
        out = work / target / "valid"
        args = [str(bin_dir / target), "--strict", "--no-main", "--root", str(work), "-o", str(out)]
        if target == "k2go":
            args += ["--pkg", "main"]
        run(args + [str(source)])
        executable = out / "check"
        if target == "k2go":
            (out / "main.go").write_text('''package main
import "os"
func main() {
    mode := os.Args[1]
    if mode == "valid" {
        if Check_Read(0) != 7 || Check_Write(0) != 7 {
            panic("valid index")
        }
        return
    }
    switch mode {
    case "read":
        Check_Read(2)
    case "write":
        Check_Write(2)
    case "read_named":
        Check_ReadNamed(2)
    case "write_named":
        Check_WriteNamed(2)
    default:
        Check_Write(-1)
    }
}
''')
            run(["go", "build", "-o", str(executable), "."], cwd=out, env=dict(os.environ, GO111MODULE="off"))
        else:
            extension = "c" if target == "k2c" else "cpp"
            driver = out / f"main.{extension}"
            driver.write_text('''#include "CHECK_HEADER"
#include <string.h>
int main(int argc, char **argv) {
    (void)argc;
    if (!strcmp(argv[1], "valid")) return Read(0) != 7 || Write(0) != 7;
    if (!strcmp(argv[1], "read")) return Read(2);
    if (!strcmp(argv[1], "write")) return Write(2);
    if (!strcmp(argv[1], "read_named")) return ReadNamed(2);
    if (!strcmp(argv[1], "write_named")) return WriteNamed(2);
    return Write(-1);
}
'''.replace("CHECK_HEADER", "check.h" if target == "k2c" else "check.hpp"))
            run(["cc" if target == "k2c" else "c++", "-DKRYON_BOUNDS_CHECK", "-I" + str(root / "include"),
                 "-I" + str(out), str(driver), str(out / f"check.{extension}"), "-lm", "-o", str(executable)])
        run([str(executable), "valid"])
        for mode in ("read", "write", "negative", "read_named", "write_named"):
            result = subprocess.run([str(executable), mode], capture_output=True, text=True)
            assert result.returncode != 0 and "out of" in result.stderr, (target, mode, result.stderr)
print("Aggregate diagnostics and array read/write traps agree in C, C++, and Go")
