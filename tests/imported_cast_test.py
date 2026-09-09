#!/usr/bin/env python3
"""Execute imported enum values through strict portable emission on all targets."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
build = (root / sys.argv[1]).resolve()
sources = ["tests/fixtures/imported_cast.kry", "tests/fixtures/modules/counter.kry"]


def run(*command, cwd=root, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"{command}\n{result.stdout}\n{result.stderr}")


with tempfile.TemporaryDirectory(prefix="kryon-imported-cast-") as directory:
    work = Path(directory)
    for target in ("c", "cpp", "go", "js"):
        output = work / target
        options = ["--pkg", "casts"] if target == "go" else []
        run(str(build / "bin" / f"k2{target}"), "--strict", "--no-main", *options,
            "--root", str(root), "-o", str(output), *sources)
        if target in ("c", "cpp"):
            extension = "c" if target == "c" else "cpp"
            header = "h" if target == "c" else "hpp"
            runner = output / f"main.{extension}"
            runner.write_text(
                f'#include "tests/fixtures/imported_cast.{header}"\n'
                'int main(void) { return CheckEnums(); }\n')
            executable = output / "check"
            run("cc" if target == "c" else "c++",
                "-fsanitize=undefined", "-fno-sanitize-recover=undefined", "-I" + str(output),
                "-I" + str(output / "tests/fixtures/modules"),
                "-I" + str(root / "include"), str(runner),
                str(output / f"tests/fixtures/imported_cast.{extension}"),
                str(output / f"tests/fixtures/modules/counter.{extension}"),
                "-lm", "-o", str(executable))
            run(str(executable))
        elif target == "go":
            (output / "cast_test.go").write_text(
                'package casts\nimport "testing"\n'
                'func TestImportedCast(t *testing.T) {\n'
                ' if result := ImportedCast_CheckEnums(); result != 0 {\n'
                '  t.Fatal("enum value check", result)\n }\n}\n')
            run("go", "test", cwd=output, env={**os.environ, "GO111MODULE": "off"})
        else:
            for runtime_file in (root / "web").glob("*.js"):
                shutil.copyfile(runtime_file, output / runtime_file.name)
            runner = output / "check.mjs"
            runner.write_text(
                'import assert from "node:assert/strict";\n'
                'import { ImportedCast_CheckEnums } from "./tests/fixtures/imported_cast.js";\n'
                'assert.equal(ImportedCast_CheckEnums(null, undefined, undefined), 0);\n')
            run("node", str(runner))

    invalid = {
        "record_cast": ("value: Selection\n    return (Phase)value", "enum casts require"),
        "compound": ("value: Phase\n    value += value\n    return value", "enum compound assignment"),
        "implicit": ("return 1", "return type mismatch"),
        "different_enum": ("return (Other)0", "return type mismatch"),
    }
    for name, (body, diagnostic) in invalid.items():
        source = work / f"{name}.kry"
        source.write_text(
            'Phase :: enum {\n    Rest = 0\n}\n'
            'Other :: enum {\n    OtherRest = 0\n}\n'
            'Selection :: struct {\n    phase: Phase\n}\n'
            f'Check :: () -> Phase #export {{\n    {body}\n}}\n')
        for target in ("c", "cpp", "go", "js"):
            result = subprocess.run(
                [str(build / "bin" / f"k2{target}"), "--strict", "--no-main",
                 "--root", str(work), "-o", str(work / "invalid" / target), str(source)],
                cwd=root, text=True, capture_output=True)
            assert result.returncode != 0, (target, name, "invalid enum operation accepted")
            assert diagnostic in result.stderr, (target, name, result.stderr)

print("Imported enum casts execute in C, C++, Go, and JavaScript")
