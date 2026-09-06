#!/usr/bin/env python3
"""Execute the same language fixture through native C, C++, Go, and JavaScript."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / (sys.argv[1] if len(sys.argv) > 1 else "build/linux-x86_64")).resolve()


def run(*args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}\n{result.stderr}")
    return result.stdout


SOURCE = '''state {
    trace: int = 0
}
record :: (n: int) {
    trace = trace * 10 + n
}
run :: () -> int {
    trace = 0
    defer record(9)
    i: int = 0
    while i < 3 {
        i += 1
        defer record(i)
        if i == 1 {
            continue
        }
        if i == 2 {
            break
        }
    }
    return trace
}
nested :: (choose: bool) -> int {
    trace = 0
    defer record(9)
    if choose {
        value: int = 3
        defer record(value)
        defer record(4)
    } else {
        defer record(5)
        record(6)
    }
    return trace
}
branches :: (choose: bool) -> int {
    trace = 0
    defer record(7)
    if choose {
        return 11
    } else {
        return 22
    }
}
evaluate_once :: () -> int {
    trace = 0
    defer record(9)
    return next_value()
}
next_value :: () -> int {
    record(1)
    return trace
}
divide :: (x: i32) -> i32 {
    return 9 / x
}
left_shift :: (x: i32) -> i32 {
    return 1 << x
}
rounding :: (x: f32) -> f32 {
    x += 1.0
    return x
}
convert :: (x: f64) -> i32 {
    return (i32)x
}
truth :: (x: i32) -> bool {
    return (bool)x
}
bit :: (x: bool) -> u64 {
    return (u64)x
}
step :: (x: i32) -> i32 {
    trace = trace * 10 + x
    return x
}
order :: () -> i32 {
    trace = 0
    return step(1) * 10 + step(2)
}
wrap :: (x: i32) -> i32 {
    return x + 1
}
large :: () -> u64 {
    x: u64 = 18446744073709551615
    return x / 3
}
minimum :: () -> i64 {
    x: i64 = -9223372036854775808
    y: i64 = -1
    return x / y
}
narrow :: (x: i32) -> i8 {
    return (i8)x
}
shift :: (x: i32) -> i32 {
    return x >> 2
}
choose :: (yes: bool) -> i32 {
    trace = 0
    return yes ? step(3) : step(4)
}
lazy :: (yes: bool) -> bool {
    trace = 0
    return yes && step(5) > 0
}
capture :: () -> i32 {
    x: i32 = 3
    return x + x++
}
'''

with tempfile.TemporaryDirectory(prefix="kryon-language-") as directory:
    work = Path(directory)
    source = work / "cleanup.kry"
    source.write_text(SOURCE)
    for target in ("c", "cpp", "go", "js"):
        out = work / target
        args = [str(BUILD / "bin" / f"k2{target}"), "--strict", "--no-main", "--root", str(work), "-o", str(out)]
        if target == "js":
            args += ["--runtime", "./kryon-runtime.js"]
        run(*args, str(source))
        assert "defer " not in (out / f"cleanup.{target}").read_text()

    (work / "c/ui_inspect.h").write_text('''
#include <stdbool.h>
static inline void PushUIInspectSource(const char *p, int n) {(void)p; (void)n;}
static inline void PopUIInspectSource(void) {}
''')
    # Include the generated translation unit so the harness can inspect its
    # private state without changing the generated API.
    (work / "c/driver.c").write_text('''#include <stdbool.h>
#include <assert.h>
#include "cleanup.c"
int main(int argc, char **argv) {
    if(argc > 1) {
        volatile int32_t result;
        if(argv[1][0] == 'd') result = divide(0);
        else if(argv[1][0] == 's') result = left_shift(-1);
        else result = convert(2147483648.0);
        (void)result;
        return 0;
    }
    assert(rounding(16777216.0f) == 16777216.0f);
    assert(convert(-12.75) == -12);
    assert(truth(2) && !truth(0));
    assert(bit(true) == 1 && bit(false) == 0);
    assert(order() == 12 && trace == 12);
    assert(wrap(INT32_MAX) == INT32_MIN);
    assert(large() == UINT64_C(6148914691236517205));
    assert(minimum() == INT64_MIN);
    assert(narrow(255) == -1);
    assert(shift(-9) == -3);
    assert(choose(true) == 3 && trace == 3);
    assert(choose(false) == 4 && trace == 4);
    assert(!lazy(false) && trace == 0);
    assert(lazy(true) && trace == 5);
    assert(capture() == 6);
    assert(run() == 12 && trace == 129);
    assert(nested(true) == 43 && trace == 439);
    assert(nested(false) == 65 && trace == 659);
    assert(branches(true) == 11 && trace == 7);
    assert(branches(false) == 22 && trace == 7);
    assert(evaluate_once() == 1 && trace == 19);
}
''')
    run(os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", str(work / "c/driver.c"), "-o", str(work / "c/test"))
    run(str(work / "c/test"))
    shutil.copyfile(work / "c/ui_inspect.h", work / "cpp/ui_inspect.h")
    (work / "cpp/driver.cpp").write_text((work / "c/driver.c").read_text().replace('"cleanup.c"', '"cleanup.cpp"'))
    run(os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", str(work / "cpp/driver.cpp"), "-o", str(work / "cpp/test"))
    run(str(work / "cpp/test"))
    for target in ("c", "cpp"):
        for invalid_operation in ("divide", "shift", "convert"):
            result = subprocess.run([str(work / target / "test"), invalid_operation], cwd=work, capture_output=True)
            assert result.returncode != 0, (target, invalid_operation)
    (work / "go/go.mod").write_text("module language.test\n\ngo 1.22\n")
    (work / "go/cleanup_test.go").write_text('''package krygen
import "testing"
func TestCleanup(t *testing.T) {
    s := &CleanupState{}
    if Cleanup_Rounding(s, 16777216) != 16777216 { t.Fatal("f32 rounding") }
    for _, fail := range []func(){func(){ Cleanup_Divide(s, 0) }, func(){ Cleanup_LeftShift(s, -1) }, func(){ Cleanup_Convert(s, 2147483648) }} {
        func() { defer func(){ if recover() == nil { t.Fatal("missing arithmetic trap") } }(); fail() }()
    }
    if Cleanup_Convert(s, -12.75) != -12 || !Cleanup_Truth(s, 2) || Cleanup_Truth(s, 0) || Cleanup_Bit(s, true) != 1 || Cleanup_Bit(s, false) != 0 { t.Fatal("casts") }
    if Cleanup_Order(s) != 12 || s.Trace != 12 { t.Fatal("evaluation order") }
    if Cleanup_Wrap(s, 2147483647) != -2147483648 { t.Fatal("overflow") }
    if Cleanup_Large(s) != 6148914691236517205 { t.Fatal("u64 division") }
    if Cleanup_Minimum(s) != -9223372036854775808 { t.Fatal("signed division") }
    if Cleanup_Narrow(s, 255) != -1 { t.Fatal("narrowing") }
    if Cleanup_Shift(s, -9) != -3 { t.Fatal("arithmetic shift") }
    if Cleanup_Choose(s, true) != 3 || s.Trace != 3 { t.Fatal("conditional") }
    if Cleanup_Choose(s, false) != 4 || s.Trace != 4 { t.Fatal("conditional") }
    if Cleanup_Lazy(s, false) || s.Trace != 0 { t.Fatal("short circuit") }
    if !Cleanup_Lazy(s, true) || s.Trace != 5 { t.Fatal("short circuit") }
    if Cleanup_Capture(s) != 6 { t.Fatal("operand capture") }
    if Cleanup_Run(s) != 12 || s.Trace != 129 { t.Fatal("loop cleanup", s) }
    if Cleanup_Nested(s, true) != 43 || s.Trace != 439 { t.Fatal("nested cleanup", s) }
    if Cleanup_Nested(s, false) != 65 || s.Trace != 659 { t.Fatal("else cleanup", s) }
    if Cleanup_Branches(s, true) != 11 || s.Trace != 7 { t.Fatal("return cleanup", s) }
    if Cleanup_Branches(s, false) != 22 || s.Trace != 7 { t.Fatal("return cleanup", s) }
    if Cleanup_EvaluateOnce(s) != 1 || s.Trace != 19 { t.Fatal("return evaluation", s) }
}
''')
    run("go", "test", "./...", cwd=work / "go")
    shutil.copyfile(ROOT / "web/kryon-runtime.js", work / "js/kryon-runtime.js")
    (work / "js/package.json").write_text('{"type":"module"}\n')
    (work / "js/test.mjs").write_text('''import assert from "node:assert/strict";
import * as m from "./cleanup.js";
const s = m.createState();
assert.equal(m.Cleanup_Order(), 12); assert.equal(m.moduleState.trace, 12);
assert.equal(m.Cleanup_Rounding(null, s, null, 16777216), 16777216);
assert.throws(() => m.Cleanup_Divide(null, s, null, 0), RangeError);
assert.throws(() => m.Cleanup_LeftShift(null, s, null, -1), RangeError);
assert.equal(m.Cleanup_Convert(null, s, null, -12.75), -12);
assert.equal(m.Cleanup_Truth(null, s, null, 2), true);
assert.equal(m.Cleanup_Truth(null, s, null, 0), false);
assert.equal(m.Cleanup_Bit(null, s, null, true), 1n);
assert.equal(m.Cleanup_Bit(null, s, null, false), 0n);
assert.throws(() => m.Cleanup_Convert(null, s, null, NaN), RangeError);
assert.throws(() => m.Cleanup_Convert(null, s, null, Infinity), RangeError);
assert.throws(() => m.Cleanup_Convert(null, s, null, 2147483648), RangeError);
assert.equal(m.Cleanup_Order(null, s), 12); assert.equal(s.trace, 12);
assert.equal(m.Cleanup_Wrap(null, s, null, 2147483647), -2147483648);
assert.equal(m.Cleanup_Large(null, s), 6148914691236517205n);
assert.equal(m.Cleanup_Minimum(null, s), -9223372036854775808n);
assert.equal(m.Cleanup_Narrow(null, s, null, 255), -1);
assert.equal(m.Cleanup_Shift(null, s, null, -9), -3);
assert.equal(m.Cleanup_Choose(null, s, null, true), 3); assert.equal(s.trace, 3);
assert.equal(m.Cleanup_Choose(null, s, null, false), 4); assert.equal(s.trace, 4);
assert.equal(m.Cleanup_Lazy(null, s, null, false), false); assert.equal(s.trace, 0);
assert.equal(m.Cleanup_Lazy(null, s, null, true), true); assert.equal(s.trace, 5);
assert.equal(m.Cleanup_Capture(null, s), 6);
assert.equal(m.Cleanup_Run(null, s), 12); assert.equal(s.trace, 129);
assert.equal(m.Cleanup_Nested(null, s, null, true), 43); assert.equal(s.trace, 439);
assert.equal(m.Cleanup_Nested(null, s, null, false), 65); assert.equal(s.trace, 659);
assert.equal(m.Cleanup_Branches(null, s, null, true), 11); assert.equal(s.trace, 7);
assert.equal(m.Cleanup_Branches(null, s, null, false), 22); assert.equal(s.trace, 7);
assert.equal(m.Cleanup_EvaluateOnce(null, s), 1); assert.equal(s.trace, 19);
''')
    run("node", str(work / "js/test.mjs"))

    invalid = {
        "literal_range": ('bad :: () -> i8 {\n return 128\n}\n', "integer literal does not fit"),
        "undefined": ('bad :: () -> int {\n return missing\n}\n', "unresolved name"),
        "return_type": ('bad :: () -> bool {\n return 123\n}\n', "return type mismatch"),
        "assignment": ('bad :: () {\n n: int = 0\n n = true\n}\n', "assignment type mismatch"),
        "arguments": ('one :: (x: int) -> int {\n return x\n}\nbad :: () -> int {\n return one(1, 2)\n}\n', "argument count mismatch"),
        "external_arguments": ('host_value :: (x: int) -> int #extern "host.Value"\nbad :: () -> int {\n return host_value(true)\n}\n', "argument type mismatch"),
        "destination": ('bad :: () {\n 1 = 2\n}\n', "assignable destination"),
        "float_bits": ('bad :: () -> double {\n return 1.5 & 2.5\n}\n', "integer operands required"),
        "cleanup_return": ('bad :: () {\n defer return\n}\n', "defer requires one expression"),
        "scope": ('bad :: () -> int {\n if true {\n n: int = 2\n }\n return n\n}\n', "unresolved name"),
        "expression": ('bad :: () -> int {\n return 1 +\n}\n', "expression is not supported"),
        "cleanup_jump": ('bad :: () {\n defer done()\n goto end\n end:\n}\n', "goto and labels"),
        "cleanup_shadow": ('done :: (n: int) {\n unused n\n}\nbad :: (n: int) {\n defer done(n)\n if true {\n n: int = 3\n }\n}\n', "cannot shadow"),
    }
    for name, (text, diagnostic) in invalid.items():
        path = work / f"{name}.kry"
        path.write_text(text)
        for target in ("c", "cpp", "go", "js"):
            result = subprocess.run([str(BUILD / "bin" / f"k2{target}"), "--strict", "--root", str(work), "-o", str(work / "invalid"), str(path)], text=True, capture_output=True)
            assert result.returncode != 0, (name, target, result.stdout)
            assert diagnostic in result.stderr, (name, target, result.stderr)
            assert f"{name}.kry:" in result.stderr

print("language semantics: C, C++, Go, JavaScript behavior and diagnostics agree")
