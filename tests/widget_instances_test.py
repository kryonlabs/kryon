#!/usr/bin/env python3
"""Run declared instance state on the real C, C++, Go, and JS render hosts."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build")
parser.add_argument("--cc", default="cc")
parser.add_argument("--cppflags", default="")
parser.add_argument("--ldflags", required=True)
args = parser.parse_args()
BIN = (ROOT / args.build / "bin").resolve()


def run(*command, cwd=ROOT):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"{command}\n{result.stdout}\n{result.stderr}")


C_DRIVER = '''#include "widget_instances.h"
#include "widget_instance_calls.h"
#include "toolkit_store.h"
#include <assert.h>
static int count(uint64_t key, int amount) {
    CounterProps props = {key, amount};
    return Counter(props);
}
int main(void) {
    ToolkitStore *first = toolkit_store_new();
    ToolkitStore *second = toolkit_store_new();
    ToolkitStore *previous = toolkit_store_swap(first);
    assert(count(7, 3) == 3);
    assert(count(8, 5) == 5);
    assert(CounterBlock() == 13);
    assert(ReadCounter(8) == 8 && ReadCounter(7) == 5);
    assert(ReadForwardedCounter(7) == 5);
    assert(count((UINT64_C(1) << 60) + 7, 9) == 9 && ReadCounter(7) == 5);
    assert(OtherCounter(7) == 1 && ReadCounter(7) == 5);
    assert(CheckInstanceReferences() == 0);
    toolkit_store_swap(second);
    assert(ReadCounter(7) == 0 && count(7, 21) == 21);
    toolkit_store_swap(first);
    assert(ReadCounter(7) == 5);
    for(int frame = 0; frame < 14; frame++) {
        toolkit_store_frame(first);
    }
    assert(ReadCounter(7) == 0);
    toolkit_store_swap(second);
    assert(ReadCounter(7) == 21);
    toolkit_store_swap(previous);
    toolkit_store_free(first);
    toolkit_store_free(second);
    return 0;
}
'''

GO_DRIVER = '''package instances
import (
    "testing"
    kryon "github.com/waozixyz/kryon/go/kryon"
)
func TestInstances(t *testing.T) {
    first, second := kryon.New(kryon.AppConfig{}), kryon.New(kryon.AppConfig{})
    defer first.Close()
    defer second.Close()
    kryon.SetRuntime(first)
    count := func(key uint64, amount int32) int32 {
        return WidgetInstances_Counter(CounterProps{Key: key, Amount: amount})
    }
    if count(7, 3) != 3 || count(8, 5) != 5 || WidgetInstances_CounterBlock() != 13 {
        t.Fatal("block and ordinary calls did not share declaration state")
    }
    if WidgetInstances_ReadCounter(8) != 8 || WidgetInstances_ReadCounter(7) != 5 ||
       WidgetInstances_OtherCounter(7) != 1 || WidgetInstances_ReadCounter(7) != 5 ||
       WidgetInstances_CheckInstanceReferences() != 0 {
        t.Fatal("typed instance bindings lost their identity or value semantics")
    }
    if count((1<<60)+7, 9) != 9 || WidgetInstances_ReadCounter(7) != 5 {
        t.Fatal("wide instance key was truncated")
    }
    if WidgetInstanceCalls_ReadForwardedCounter(7) != 5 {
        t.Fatal("imported declaration used a different instance")
    }
    kryon.SetRuntime(second)
    if WidgetInstances_ReadCounter(7) != 0 || count(7, 21) != 21 {
        t.Fatal("state leaked between hosts")
    }
    kryon.SetRuntime(first)
    if WidgetInstances_ReadCounter(7) != 5 {
        t.Fatal("host state was reset")
    }
    for frame := 0; frame < 14; frame++ {
        first.BeginFrame()
        first.EndFrame()
    }
    if WidgetInstances_ReadCounter(7) != 0 {
        t.Fatal("removed instance did not expire")
    }
    kryon.SetRuntime(second)
    if WidgetInstances_ReadCounter(7) != 21 {
        t.Fatal("another host expired this instance")
    }
}
'''

JS_DRIVER = '''import assert from "node:assert/strict";
import * as host from "./kryon-runtime.js";
import * as widget from "./widget_instances.js";
import * as caller from "./widget_instance_calls.js";
const first = host.createRuntime(), second = host.createRuntime();
const count = (rt, key, amount) => widget.WidgetInstances_Counter(rt, null, null, {key, amount});
const read = (rt, key) => widget.WidgetInstances_ReadCounter(rt, null, null, key);
assert.equal(count(first, 7n, 3), 3);
assert.equal(count(first, 8n, 5), 5);
assert.equal(widget.WidgetInstances_CounterBlock(first), 13);
assert.equal(read(first, 8n), 8);
assert.equal(caller.WidgetInstanceCalls_ReadForwardedCounter(first, null, null, 7n), 5);
assert.equal(read(first, 7n), 5);
assert.equal(count(first, (1n << 60n) + 7n, 9), 9);
assert.equal(read(first, 7n), 5);
assert.equal(widget.WidgetInstances_OtherCounter(first, null, null, 7n), 1);
assert.equal(read(first, 7n), 5);
assert.equal(widget.WidgetInstances_CheckInstanceReferences(first), 0);
assert.equal(read(second, 7n), 0);
assert.equal(count(second, 7n, 21), 21);
assert.equal(read(first, 7n), 5);
for (let frame = 0; frame < 14; frame++) {
    host.beginFrame(first);
    host.endFrame(first);
}
assert.equal(read(first, 7n), 0);
assert.equal(read(second, 7n), 21);
'''

GO_INTERNAL_DRIVER = '''package kryon
import "testing"
func TestGeneratedHostMethods(t *testing.T) {
    first := New(AppConfig{}).(*runtime)
    second := New(AppConfig{}).(*runtime)
    // A runtime implementation must use its receiver, not the active app host.
    SetRuntime(second)
    if first.WidgetInstances_Counter(CounterProps{Key: 7, Amount: 3}) != 3 ||
       first.WidgetInstances_CounterBlock() != 8 ||
       first.WidgetInstances_ReadCounter(7) != 5 ||
       first.WidgetInstanceCalls_ReadForwardedCounter(7) != 5 {
        t.Fatal("transitive instance calls lost their receiver")
    }
    if second.WidgetInstances_ReadCounter(7) != 0 ||
       first.WidgetInstances_CheckInstanceReferences() != 0 {
        t.Fatal("generated state bindings used another host or lost reference semantics")
    }
}
'''

with tempfile.TemporaryDirectory(prefix="kryon-widget-instances-") as directory:
    work = Path(directory)
    source = work / "widget_instances.kry"
    shutil.copyfile(ROOT / "tests/fixtures/widget_instances.kry", source)
    caller = work / "widget_instance_calls.kry"
    shutil.copyfile(ROOT / "tests/fixtures/widget_instance_calls.kry", caller)
    for target in ("c", "cpp", "go", "js"):
        output = work / target
        flags = (["--pkg", "instances"] if target == "go" else
                 ["--runtime", "./kryon-runtime.js"] if target == "js" else [])
        run(str(BIN / f"k2{target}"), "--strict", "--no-main", *flags,
            "--root", str(work), "-o", str(output), str(source), str(caller))
        if target in ("c", "cpp"):
            driver = output / f"main.{target}"
            native_driver = C_DRIVER
            if target == "cpp":
                for header in ("widget_instances", "widget_instance_calls"):
                    native_driver = native_driver.replace(f'"{header}.h"', f'"{header}.hpp"')
            driver.write_text(native_driver)
            compiler = shlex.split(args.cc) if target == "c" else [os.environ.get("CXX", "c++")]
            run(*compiler, *shlex.split(args.cppflags), f"-I{output}", f"-I{ROOT / 'src/ui'}",
                str(driver), str(output / f"widget_instances.{target}"),
                str(output / f"widget_instance_calls.{target}"),
                *shlex.split(args.ldflags), "-o", str(output / "check"))
            run(str(output / "check"))
        elif target == "go":
            (output / "go.mod").write_text(
                "module instances\n\ngo 1.25.0\n"
                "require github.com/waozixyz/kryon/go/kryon v0.0.0\n"
                f"replace github.com/waozixyz/kryon/go/kryon => {ROOT / 'go/kryon'}\n")
            (output / "instances_test.go").write_text(GO_DRIVER)
            run("go", "test", "-mod=mod", "./...", cwd=output)
        else:
            for runtime_file in (ROOT / "web").glob("*.js"):
                shutil.copyfile(runtime_file, output / runtime_file.name)
            (output / "package.json").write_text('{"type":"module"}\n')
            (output / "check.mjs").write_text(JS_DRIVER)
            run("node", str(output / "check.mjs"))
    internal = work / "runtime"
    internal.mkdir()
    for runtime_file in (ROOT / "go/kryon").iterdir():
        if (runtime_file.suffix == ".go" and not runtime_file.name.endswith("_test.go")) or runtime_file.name in ("go.mod", "go.sum"):
            shutil.copyfile(runtime_file, internal / runtime_file.name)
    run(str(BIN / "k2go"), "--strict", "--no-main", "--runtime-implementation", "--pkg", "kryon",
        "--root", str(work), "-o", str(internal), str(source), str(caller))
    (internal / "instances_test.go").write_text(GO_INTERNAL_DRIVER)
    run("go", "test", "./...", cwd=internal)
    for replacement, diagnostic in (
        ("retained: i32 #instance(props.key)", "requires a declared record"),
        ("retained: CounterState #instance(true)", "key requires an integer"),
        ("retained: CounterState #instance()", "key requires an integer"),
    ):
        invalid = source.read_text().replace("retained: CounterState #instance(props.key)", replacement)
        bad = work / "invalid.kry"
        bad.write_text(invalid)
        for target in ("c", "cpp", "go", "js"):
            result = subprocess.run([str(BIN / f"k2{target}"), "--no-main", "--root", str(work),
                                     "-o", str(work / "invalid"), str(bad)], text=True, capture_output=True)
            assert result.returncode != 0 and diagnostic in result.stderr, (target, result.stderr)
print("widget instances: C, C++, Go, and JavaScript preserve state, types, hosts, and lifetime")
