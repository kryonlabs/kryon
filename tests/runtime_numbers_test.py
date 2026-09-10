#!/usr/bin/env python3
"""Runtime modules share numeric support without changing arithmetic or app output."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / sys.argv[1]).resolve()


def run(*args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    assert result.returncode == 0, f"{args}:\n{result.stdout}\n{result.stderr}"
    return result.stdout


with tempfile.TemporaryDirectory(prefix="kryon-runtime-numbers-") as directory:
    work = Path(directory)
    first = work / "first.kry"
    second = work / "second.kry"
    first.write_text('''#module "first"
Wrap :: (x: u8) -> u8 {
    return x + (u8)1
}
Divide :: (a: i32, b: i32) -> i32 {
    return a / b
}
''')
    second.write_text('''#module "second"
#import "first"
Again :: (x: u8) -> u8 {
    return Wrap(x)
}
Shift :: (a: u8, b: u8) -> u8 {
    return a << b
}
Convert :: (x: f64) -> i32 {
    return (i32)x
}
''')
    output = work / "runtime"
    command = [str(BUILD / "bin/k2go"), "--strict", "--no-main", "--runtime-implementation",
               "--pkg", "numbers", "--root", str(work), "-o", str(output)]
    run(*command, str(first), str(second))
    support = output / "numeric_support.go"
    generated = "\n".join(path.read_text() for path in output.glob("*.go"))
    assert generated.count("func number_runtime_bits(") == 1
    assert generated.count("func number_runtime_float(") == 1
    assert not re.search(r"number_[0-9a-f]{8}_", generated)
    previous_support = support.read_text()
    # Regenerating one module must not create another definition or invalidate
    # modules already emitted into the same runtime package.
    run(*command, str(first))
    assert support.read_text() == previous_support
    (output / "go.mod").write_text("module numbers\n\ngo 1.25.0\n")
    (output / "numbers_test.go").write_text('''package numbers
import ("math"; "testing")
func TestArithmetic(t *testing.T) {
    if First_Wrap(255) != 0 || Second_Again(255) != 0 || Second_Shift(128, 1) != 0 {
        t.Fatal("unsigned wrapping changed")
    }
    if First_Divide(-7, 2) != -3 || First_Divide(-2147483648, -1) != -2147483648 {
        t.Fatal("signed division changed")
    }
    if Second_Convert(-17.75) != -17 || Second_Convert(2147483647) != 2147483647 {
        t.Fatal("float conversion changed")
    }
    for _, operation := range []func(){
        func(){First_Divide(1, 0)}, func(){Second_Shift(1, 8)},
        func(){Second_Convert(2147483648)}, func(){Second_Convert(math.NaN())},
        func(){Second_Convert(math.Inf(1))},
    } {
        func(){
            defer func(){if recover() == nil { t.Error("invalid operation did not panic") }}()
            operation()
        }()
    }
}
''')
    run("gofmt", "-w", *[str(path) for path in output.glob("*.go")])
    run("go", "test", "./...", cwd=output)
    app = work / "app"
    run(str(BUILD / "bin/k2go"), "--strict", "--no-main", "--pkg", "numbers",
        "--root", str(work), "-o", str(app), str(first))
    assert not (app / "numeric_support.go").exists()
    assert "func number_" in (app / "first.go").read_text()
    run("go", "test", str(app / "first.go"))
    reserved = work / "numeric_support.kry"
    reserved.write_text(first.read_text())
    result = subprocess.run(command + [str(reserved)], text=True, capture_output=True)
    assert result.returncode != 0 and "duplicate source basename numeric_support" in result.stderr

print("Runtime numeric support is shared; arithmetic and standalone app output pass")
