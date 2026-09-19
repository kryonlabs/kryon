#!/usr/bin/env python3
"""Execute borrowed slice aliasing and release bounds checks on native targets."""
from pathlib import Path
import os
import resource
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bin_dir = Path(sys.argv[1]).resolve()


def run(args, **kwargs):
    result = subprocess.run(args, capture_output=True, text=True, **kwargs)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result


resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix="kryon-slice-values-") as directory:
    work = Path(directory)
    invalid = {
        "local_return": "Bad :: () -> []i32 {\na: [2]i32\nreturn a[:]\n}",
        "copied_parameter": "Bad :: (a: [2]i32) -> []i32 {\nreturn a[:]\n}",
        "inner_escape": "Bad :: () {\nview: []i32\nif true {\na: [2]i32\nview = a[:]\n}\n}",
        "helper_escape": "Keep :: (a: []i32) -> []i32 {\nreturn a\n}\nBad :: () -> []i32 {\na: [2]i32\nreturn Keep(a[:])\n}",
        "parameter_escape": "Bad :: (view: []i32) -> []i32 {\na: [2]i32\nview = a[:]\nreturn view\n}",
        "global_storage": "view :: []i32 #global",
        "state_storage": "state {\nview: []i32\n}",
        "readonly_length": "Bad :: () {\na: [2]i32\nv := a[:]\nv.length = 1\n}",
        "wrong_bound": "Bad :: () {\na: [2]i32\nv := a[0.5:]\n}",
        "comparison": "Bad :: () {\na: [2]i32\nv := a[:]\nsame := v == v\n}",
        "cast": "Bad :: () {\na: [2]i32\nv := a[:]\nx := (i32)v\n}",
        "foreign": "Read :: (view: []i32) #extern",
        "late_helper": "Bad :: () -> []i32 {\na: [2]i32\nreturn Keep(a[:])\n}\nKeep :: (view: []i32) -> []i32 {\nreturn view\n}",
        "captured_rebinding": "Action :: () #slot\nBad :: () {\na: [2]i32\nview := a[:]\nchange: Action = () #slot {\nview = a[:1]\n}\nchange()\n}",
        "record_storage": "Bad :: struct {\nview: []i32\n}",
        "temporary_record": "Box :: struct {\na: [2]i32\n}\nMake :: () -> Box {\nreturn (Box){}\n}\nUse :: (a: []i32) -> i32 {\nreturn a.length\n}\nBad :: () {\nUse(Make().a[:])\n}",
    }
    for name, source_text in invalid.items():
        source = work / f"{name}.kry"
        source.write_text(source_text + "\n")
        for target in ("k2c", "k2cpp", "k2go"):
            for mode in ([], ["--strict"]):
                result = subprocess.run(
                    [str(bin_dir / target), *mode, "--no-main", "--root", str(work),
                     "-o", str(work / name / target), str(source)], capture_output=True, text=True)
                assert result.returncode != 0, (name, target, mode, "unsafe slice accepted")
                assert any(word in result.stderr for word in ("slice", "array", "read-only")), (name, target, mode, result.stderr)
                assert f"{name}.kry:" in result.stderr, (name, target, "missing location")

    for target in ("k2c", "k2cpp", "k2go"):
        out = work / target
        args = [str(bin_dir / target), "--strict", "--no-main", "--root", str(root), "-o", str(out)]
        if target == "k2go":
            args += ["--pkg", "main"]
        run(args + [str(root / "tests/fixtures/slice_values.kry"),
                    str(root / "tests/fixtures/slice_helpers.kry")])
        executable = out / "check"
        if target == "k2go":
            (out / "main.go").write_text('''package main
import "os"
func main() {
    switch os.Args[1] {
    case "valid":
        if result := SliceValues_Check(); result != 0 { panic(result) }
        if SliceValues_Read(1) != 9 { panic("valid read") }
    case "read": SliceValues_Read(2)
    case "write": SliceValues_Write(2)
    case "low": SliceValues_Range(-1, 1)
    case "negative": SliceValues_Read(-1)
    case "range": SliceValues_Range(0, 3)
    case "reverse": SliceValues_Range(2, 1)
    }
}
''')
            run(["go", "build", "-o", str(executable), "."], cwd=out,
                env=dict(os.environ, GO111MODULE="off", GOCACHE="/tmp/kryon-text-go-cache"))
        else:
            cpp = target == "k2cpp"
            extension = "cpp" if cpp else "c"
            driver = out / f"main.{extension}"
            driver.write_text('''#include "tests/fixtures/slice_values.HEADER"
#include <string.h>
int main(int argc, char **argv) {
    if(argc != 2) return 99;
    if(!strcmp(argv[1], "valid")) {
        int result = Check();
        return result ? result : (Read(1) != 9);
    }
    if(!strcmp(argv[1], "read")) Read(2);
    if(!strcmp(argv[1], "write")) Write(2);
    if(!strcmp(argv[1], "low")) Range(-1, 1);
    if(!strcmp(argv[1], "negative")) Read(-1);
    if(!strcmp(argv[1], "range")) Range(0, 3);
    if(!strcmp(argv[1], "reverse")) Range(2, 1);
    return 0;
}
'''.replace("HEADER", "hpp" if cpp else "h"))
            run(["c++" if cpp else os.environ.get("CC", "cc"),
                 "-std=c++11" if cpp else "-std=c99", "-O2", "-DNDEBUG", "-Wall", "-Werror",
                 "-I" + str(out), "-I" + str(root / "include"), str(driver),
                 str(out / f"tests/fixtures/slice_values.{extension}"),
                 str(out / f"tests/fixtures/slice_helpers.{extension}"), "-o", str(executable)])
        run([str(executable), "valid"])
        for mode in ("read", "write", "low", "negative", "range", "reverse"):
            result = subprocess.run([str(executable), mode], capture_output=True, text=True)
            assert result.returncode != 0, (target, mode, "invalid bounds did not trap")
    print("Slices: aliasing, returned views, rebinding, empty values, order and release bounds pass")
