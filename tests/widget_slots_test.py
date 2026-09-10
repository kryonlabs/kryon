#!/usr/bin/env python3
"""Execute typed child-content parameters through every generated backend."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / (sys.argv[1] if len(sys.argv) > 1 else "build/linux-x86_64")).resolve()


def run(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"{args}\n{result.stdout}\n{result.stderr}")
    return result


PROVIDER = '''#module "slots"
Content :: (placement: Placement, index: i32) #slot
Placement :: struct {
    x: i32
}
Empty :: () #slot
Render :: (content: Content, empty: Empty) #ui {
    placement: Placement = (Placement){.x = 7}
    content(placement, 1)
    content(placement, 2)
    empty()
}
'''
CALLER = '''#module "caller"
#import "slots"
Forward :: (Child: Content, empty: Empty) {
    choose: bool = true
    alias: Content = choose ? Child : Child
    Render(alias, empty)
    Child((Placement){.x = 9}, 3)
}
Child :: (placement: Placement, index: i32) {
    return
}
'''

with tempfile.TemporaryDirectory(prefix="kryon-widget-slots-") as directory:
    work = Path(directory)
    provider = work / "slots.kry"
    caller = work / "caller.kry"
    provider.write_text(PROVIDER)
    caller.write_text(CALLER)
    for target in ("c", "cpp", "go", "js"):
        output = work / target
        flags = ["--runtime", "./kryon-runtime.js"] if target == "js" else []
        command = [str(BUILD / "bin" / f"k2{target}"), "--no-main", *flags,
                   "--root", str(work), "-o", str(output)]
        for sources in ((caller, provider), (provider, caller)):
            run(*command, "--strict", *map(str, sources))
            if target in ("c", "cpp"):
                (output / "ui_inspect.h").write_text("")
                header = "h" if target == "c" else "hpp"
                driver = output / f"driver.{target}"
                driver.write_text(f'''#include "caller.{header}"
static int sum;
static void child(void *context, Placement placement, int32_t index) {{
    int *total = (int *)context;
    *total += placement.x * index;
    placement.x = 100;
}}
static void empty(void *context) {{ *(int *)context += 1; }}
int main(void) {{
    Content content = {{&sum, child}};
    Empty end = {{&sum, empty}};
    caller_Forward(content, end);
    return sum != 49;
}}
''')
                compiler = os.environ.get("CC", "cc") if target == "c" else os.environ.get("CXX", "c++")
                run(compiler, str(driver), str(output / f"slots.{target}"),
                    str(output / f"caller.{target}"), "-o", str(output / "test"))
                run(str(output / "test"))
            elif target == "go":
                driver = output / "slots_test.go"
                driver.write_text('''package krygen
import "testing"
func TestSlots(t *testing.T) {
    sum := int32(0)
    Caller_Forward(func(placement Placement, index int32) {
        sum += placement.X * index
        placement.X = 100
    }, func() { sum++ })
    if sum != 49 { t.Fatalf("slot result = %d", sum) }
}
''')
                run("gofmt", "-w", str(driver))
                run("go", "test", str(output / "slots.go"), str(output / "caller.go"), str(driver))
            else:
                for runtime_file in (ROOT / "web").glob("*.js"):
                    shutil.copyfile(runtime_file, output / runtime_file.name)
                (output / "package.json").write_text('{"type":"module"}\n')
                driver = output / "test.mjs"
                driver.write_text('''import { Caller_Forward } from "./caller.js";
let sum = 0;
Caller_Forward(null, undefined, undefined, (placement, index) => {
    sum += placement.x * index;
    placement.x = 100;
}, () => { sum++; });
if (sum !== 49) throw new Error(`slot result = ${sum}`);
''')
                run("node", str(driver))

        invalid = {
            "argument type": (PROVIDER.replace("content(placement, 1)", "content(placement, true)"), "argument type mismatch"),
            "argument count": (PROVIDER.replace("content(placement, 1)", "content(placement)"), "argument count mismatch"),
            "return type": (PROVIDER.replace("Empty :: () #slot", "Empty :: () -> i32 #slot"), "bodyless void signature"),
            "body": (PROVIDER.replace("Empty :: () #slot", "Empty :: () #slot {\n}"), "bodyless void signature"),
            "unknown type": (PROVIDER.replace("index: i32) #slot", "index: Missing) #slot"), "invalid slot parameter"),
            "void type": (PROVIDER.replace("index: i32) #slot", "index: void) #slot"), "invalid slot parameter"),
            "comparison": (PROVIDER.replace("    content(placement, 1)", "    same: bool = content == content\n    content(placement, 1)"), "slot parameters require a fully checked portable body"),
            "uninitialized binding": (PROVIDER.replace("    content(placement, 1)", "    missing: Content\n    content(placement, 1)"), "slot bindings require an initializer"),
            "escaping return": (PROVIDER + "Escape :: (child: Content) -> Content {\n    return child\n}\n", "cannot escape through returns"),
            "stored field": (PROVIDER + "Stored :: struct {\n    child: Content\n}\n", "cannot be stored in records"),
            "stored state": (PROVIDER + "state {\n    child: Content\n}\n", "cannot be stored in globals or state"),
            "duplicate parameter": (PROVIDER.replace("index: i32) #slot", "placement: i32) #slot"), "duplicate slot parameter"),
        }
        for name, (source, diagnostic) in invalid.items():
            provider.write_text(source)
            for strict in ([], ["--strict"]):
                result = subprocess.run([*command, *strict, str(caller), str(provider)],
                                        text=True, capture_output=True)
                if result.returncode == 0 or diagnostic not in result.stderr:
                    raise AssertionError(f"{target}: {name}: {result.stdout}\n{result.stderr}")
        provider.write_text(PROVIDER)
        caller.write_text(CALLER + "Invalid :: () {\n    Forward(1, 2)\n}\n")
        for strict in ([], ["--strict"]):
            result = subprocess.run([*command, *strict, str(caller), str(provider)],
                                    text=True, capture_output=True)
            if result.returncode == 0 or "argument type mismatch" not in result.stderr:
                raise AssertionError(f"{target}: ordinary slot contract: {result.stderr}")
        caller.write_text(CALLER)
        print(f"{target}: typed slot invocation, forwarding, copies, and diagnostics passed")
