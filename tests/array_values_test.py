#!/usr/bin/env python3
"""Execute array value copies, ordering, callbacks and bounds on native targets."""
from pathlib import Path
import os
import resource
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
bin_dir = Path(sys.argv[1]).resolve()
targets = ("k2c", "k2cpp", "k2go")


def run(args, **kwargs):
    result = subprocess.run(args, capture_output=True, text=True, **kwargs)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result


invalid = {
    "count": ("a: [1]i32 = {1, 2}", "too many array initializer elements"),
    "element": ('a: [2]i32 = {"bad"}', "array initializer element type mismatch"),
    "named": ("a: [2]i32 = {.first = 1}", "positional elements"),
    "shape": ("a: [2]i32\nb: [3]i32 = a", "initializer type mismatch"),
    "element_copy": ("a: [2]i32\nb: [2]u32 = a", "initializer type mismatch"),
    "copy_shape": ("a: [2]i32\nb: [3]i32\na = b", "assignment type mismatch"),
    "compound": ("a: [2]i32\na += a", "array compound assignment"),
    "comparison": ("a: [2]i32\nsame: bool = a == a", "array values do not support binary operations"),
    "cast_from_array": ("a: [2]i32\nvalue: i32 = (i32)a", "array casts are not supported"),
    "cast_to_array": ("a: [2]i32 = ([2]i32)1", "array casts are not supported"),
    "zero": ("a: [0]i32", "positive capacity"),
    "nested": ("a: [2][3]i32", "nested fixed arrays"),
    "void": ("a: [2]void", "void type"),
    "unknown": ("a: [2]Missing", "unknown stored type"),
    "slice": ("a: []i32", "slices do not yet have portable storage semantics"),
    "stored_slot": ("a: [2]Action", "slot values cannot be stored"),
    "symbolic_count": ("a: [CAPACITY]i32 = {1, 2, 3}", "too many array initializer elements"),
    "unknown_bound": ("a: [MISSING]i32", "array capacity requires a known integer constant"),
}

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix="kryon-array-values-") as directory:
    work = Path(directory)
    for name, (body, diagnostic) in invalid.items():
        source = work / f"{name}.kry"
        source.write_text("CAPACITY :: 2\nAction :: () #slot\nCheck :: () {\n" + body + "\n}\n")
        for target in targets:
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                 "-o", str(work / name / target), str(source)],
                capture_output=True, text=True,
            )
            assert result.returncode != 0, (target, name, "invalid array accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)
            assert f"{name}.kry:" in result.stderr, (target, name, "missing source location")

    invalid_bounds = {
        "zero_bound": "0",
        "negative_bound": "-1",
        "large_bound": "1048577",
        "float_bound": "2.5",
        "division_bound": "2 / 0",
        "overflow_bound": "(2147483647 + 1) / 1073741824",
        "remainder_overflow_bound": "((-2147483647 - 1) % -1) + 2",
        "cycle_bound": "BAD + 1",
    }
    for name, expression in invalid_bounds.items():
        source = work / f"{name}.kry"
        source.write_text(f"BAD :: {expression}\nBox :: struct {{\nvalues: [BAD]i32\n}}\n")
        for target in targets:
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                 "-o", str(work / name / target), str(source)], capture_output=True, text=True,
            )
            assert result.returncode != 0, (target, name, "invalid bound accepted")
            assert "array capacity is not a valid bounded integer constant" in result.stderr, (target, name, result.stderr)
            assert f"{name}.kry:" in result.stderr, (target, name, "missing source location")

    first = work / "first.kry"
    second = work / "second.kry"
    first.write_text('#module "first"\nSHARED_COUNT :: 2\n')
    second.write_text('#module "second"\nSHARED_COUNT :: 3\n')
    for name, imports, diagnostic in (
        ("ambiguous", '#import "first"\n#import "second"\n', "not a valid bounded integer constant"),
        ("unimported", "", "requires a known integer constant"),
    ):
        source = work / f"{name}.kry"
        source.write_text(imports + "Box :: struct {\nvalues: [SHARED_COUNT]i32\n}\n")
        for target in targets:
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                 "-o", str(work / name / target), str(source), str(first), str(second)],
                capture_output=True, text=True,
            )
            assert result.returncode != 0, (target, name, "invalid import scope accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)

    invalid_calls = {
        "argument_shape": ("Take :: (a: [2]i32) {\n}\nCheck :: () {\na: [3]i32\nTake(a)\n}", "argument type mismatch"),
        "return_shape": ("Bad :: () -> [2]i32 {\na: [3]i32\nreturn a\n}", "return type mismatch"),
        "missing_return": ("Bad :: () -> [2]i32 {\n}", "return on every path"),
        "partial_return": ("Bad :: (yes: bool) -> [2]i32 {\nif yes { return ([2]i32){1} }\n}", "return on every path"),
        "bare_return": ("Bad :: () -> [2]i32 {\nreturn\n}", "return value does not match"),
        "unknown_signature": ("Bad :: (a: [MISSING]i32) {\n}", "known integer constant"),
    }
    for name, (source_text, diagnostic) in invalid_calls.items():
        source = work / f"{name}.kry"
        source.write_text(source_text + "\n")
        for target in targets:
            result = subprocess.run(
                [str(bin_dir / target), "--strict", "--no-main", "--root", str(work),
                 "-o", str(work / name / target), str(source)], capture_output=True, text=True,
            )
            assert result.returncode != 0, (target, name, "invalid array call accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)
            assert f"{name}.kry:" in result.stderr, (target, name, "missing source location")

    for target in targets:
        out = work / target
        args = [str(bin_dir / target), "--strict", "--no-main", "--root", str(root), "-o", str(out)]
        if target == "k2go":
            args += ["--pkg", "main"]
        run(args + [str(root / "tests/fixtures/array_values.kry"),
                    str(root / "tests/fixtures/record_types.kry"),
                    str(root / "tests/fixtures/array_bound_types.kry")])
        executable = out / "check"
        if target == "k2go":
            (out / "main.go").write_text('''package main
import "os"
func main() {
    switch os.Args[1] {
    case "valid":
        if result := ArrayValues_Check(); result != 0 { panic(result) }
        if ArrayValues_Read(1) != 9 || ArrayValues_Write(0) != 11 { panic("valid bounds") }
    case "read": ArrayValues_Read(2)
    case "write": ArrayValues_Write(2)
    case "negative": ArrayValues_Write(-1)
    }
}
''')
            run(["go", "build", "-o", str(executable), "."], cwd=out,
                env=dict(os.environ, GO111MODULE="off"))
        else:
            cpp = target == "k2cpp"
            header = "hpp" if cpp else "h"
            extension = "cpp" if cpp else "c"
            driver = out / f"main.{extension}"
            driver.write_text('''#include "tests/fixtures/array_values.HEADER"
#include <assert.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    if(!strcmp(argv[1], "valid")) {
        assert(Check() == 0);
        assert(Read(1) == 9 && Write(0) == 11);
    } else if(!strcmp(argv[1], "read")) Read(2);
    else if(!strcmp(argv[1], "write")) Write(2);
    else if(!strcmp(argv[1], "negative")) Write(-1);
    return 0;
}
'''.replace("HEADER", header))
            run(["c++" if cpp else os.environ.get("CC", "cc"),
                 "-std=c++11" if cpp else "-std=c99", "-Wall", "-Werror", "-DKRYON_BOUNDS_CHECK",
                 "-I" + str(out), "-I" + str(root / "include"), str(driver),
                 str(out / f"tests/fixtures/array_values.{extension}"),
                 str(out / f"tests/fixtures/array_bound_types.{extension}"), "-o", str(executable)])
        run([str(executable), "valid"])
        for mode in ("read", "write", "negative"):
            result = subprocess.run([str(executable), mode], capture_output=True, text=True)
            assert result.returncode != 0, (target, mode, "invalid index did not trap")
    print("Array values: copies, evaluation order, borrowed captures and bounds pass in C, C++, Go")
