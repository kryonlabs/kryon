#!/usr/bin/env python3
"""Execute typed child-content parameters through every generated backend."""

import os
from itertools import product
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
state {
    total: i32 = 0
    sequence: i32 = 0
}
Accumulate :: (placement: Placement, index: i32) {
    total += placement.x * index
    placement.x = 100
}
Finish :: () {
    total += 1
}
Read :: () -> i32 {
    return total
}
Reset :: () {
    total = 0
    sequence = 0
}
Next :: () -> i32 {
    sequence += 1
    return sequence
}
PanelProps :: struct {
    first: i32
    second: i32
    placement: Placement
}
Panel :: (props: PanelProps, content: Content, finish: Empty) #ui {
    content(props.placement, props.first)
    content(props.placement, props.second)
    finish()
}
Ignore :: (placement: Placement, index: i32) {
    return
}
Render :: (content: Content, empty: Empty) #ui {
    placement: Placement = (Placement){.x = 7}
    content(placement, 1)
    content(placement, 2)
    empty()
}
LocalContent :: () -> i32 {
    Render(Accumulate, Finish)
    return total
}
InlineState :: () -> i32 {
    Reset()
    child: Content = (placement: Placement, st: i32) #slot {
        total += st
    }
    Render(child, Finish)
    return total
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
Child :: (placement: Placement, index: i32) #private {
    Accumulate(placement, index)
}
OwnContent :: () -> i32 {
    Reset()
    choose: bool = false
    Forward(choose ? Accumulate : Child, Finish)
    return Read()
}
CaptureContent :: () -> i32 {
    sum: i32 = 3
    record: Placement = (Placement){.x = 4}
    draw: Content = (placement: Placement, index: i32) #slot {
        sum += placement.x * index + record.x
        next: Placement = (Placement){.x = record.x + 1}
        record = next
    }
    Forward(draw, Finish)
    return sum + record.x
}
InlineBlockContent :: () -> i32 {
    sum: i32 = 1
    Panel example: {
        first = 1
        second = 2
        placement = {7}
        content = (placement: Placement, index: i32) #slot {
            sum += placement.x * index
        }
        finish = () #slot {
            sum += 3
        }
    }
    return sum
}
CapturedSlot :: () -> i32 {
    sum: i32 = 0
    first: Empty = () #slot {
        sum += 2
    }
    outer: Empty = () #slot {
        first()
    }
    first = () #slot {
        sum += 3
    }
    outer()
    return sum
}
EarlyReturn :: () -> i32 {
    sum: i32 = 0
    child: Content = (placement: Placement, index: i32) #slot {
        if index == 2 { return }
        sum += index
    }
    Forward(child, Finish)
    return sum
}
NestedContent :: () -> i32 {
    value_0: i32 = 0
    outer: Empty = () #slot {
        inner: Empty = () #slot {
            value_0 += 5
        }
        inner()
    }
    outer()
    return value_0
}
BlockContent :: () -> i32 {
    Reset()
    Panel example: {
        first = Next()
        content = Next() == 2 ? Accumulate : Ignore
        second = Next()
        placement = {7}
        finish = Finish
    }
    return Read()
}
OrdinaryContent :: () -> i32 {
    Reset()
    props: PanelProps
    props.first = Next()
    content: Content = Next() == 2 ? Accumulate : Ignore
    props.second = Next()
    props.placement.x = 7
    Panel(props, content, Finish)
    return Read()
}
DefaultContent :: () -> i32 {
    Reset()
    Panel example: {
        finish = Finish
        content = Accumulate
    }
    return Read()
}
ImportedContent :: () -> i32 {
    Reset()
    choose: bool = false
    child: Content = choose ? Child : Accumulate
    child = Accumulate
    Forward(child, Finish)
    return Read()
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
        for sources, widget in product(((caller, provider), (provider, caller)), ("Panel", "Button")):
            provider.write_text(PROVIDER.replace("Panel ::", f"{widget} ::"))
            caller.write_text(CALLER.replace("Panel example:", f"{widget} example:").replace("Panel(props", f"{widget}(props"))
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
    return slots_InlineState() != 4 || sum != 49 || caller_OwnContent() != 49 || caller_ImportedContent() != 49 ||
        caller_BlockContent() != 29 || caller_OrdinaryContent() != 29 || caller_DefaultContent() != 1 || caller_CaptureContent() != 73 ||
        caller_InlineBlockContent() != 25 || caller_NestedContent() != 5 ||
        caller_CapturedSlot() != 3 || caller_EarlyReturn() != 4;
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
    if Caller_CaptureContent() != 73 || Caller_InlineBlockContent() != 25 || Caller_NestedContent() != 5 || Caller_CapturedSlot() != 3 || Caller_EarlyReturn() != 4 {
        t.Fatal("inline slot captures must share their lexical variables")
    }
    if Caller_BlockContent() != 29 || Caller_OrdinaryContent() != 29 || Caller_DefaultContent() != 1 {
        t.Fatal("block slots must preserve source evaluation order and omitted props")
    }
    if Slots_InlineState(&SlotsState{}) != 4 {
        t.Fatal("inline parameter must not shadow hidden module state")
    }
    if Slots_LocalContent(&SlotsState{Total: 100}) != 122 {
        t.Fatal("slot must retain the supplied module state")
    }
    if Caller_OwnContent() != 49 || Caller_ImportedContent() != 49 {
        t.Fatal("local and imported function values must bind the same slot")
    }
}
''')
                run("gofmt", "-w", str(driver))
                run("go", "test", str(output / "slots.go"), str(output / "caller.go"), str(driver))
            else:
                for runtime_file in (ROOT / "web").glob("*.js"):
                    shutil.copyfile(runtime_file, output / runtime_file.name)
                (output / "package.json").write_text('{"type":"module"}\n')
                driver = output / "test.mjs"
                driver.write_text('''import { Caller_Forward, Caller_OwnContent, Caller_ImportedContent, Caller_BlockContent, Caller_OrdinaryContent, Caller_DefaultContent, Caller_CaptureContent, Caller_InlineBlockContent, Caller_NestedContent, Caller_CapturedSlot, Caller_EarlyReturn } from "./caller.js";
import { Slots_LocalContent, Slots_InlineState } from "./slots.js";
if (Slots_InlineState(null) !== 4) throw new Error("inline parameter must not shadow hidden module state");
if (Slots_LocalContent(null, {total: 100}) !== 122)
    throw new Error("slot must retain the supplied module state");
if (Caller_BlockContent(null) !== 29 || Caller_OrdinaryContent(null) !== 29 || Caller_DefaultContent(null) !== 1)
    throw new Error("block slots must preserve source evaluation order and omitted props");
if (Caller_CaptureContent(null) !== 73 || Caller_InlineBlockContent(null) !== 25 || Caller_NestedContent(null) !== 5 || Caller_CapturedSlot(null) !== 3 || Caller_EarlyReturn(null) !== 4)
    throw new Error("inline slot captures must share their lexical variables");
let sum = 0;
Caller_Forward(null, undefined, undefined, (placement, index) => {
    sum += placement.x * index;
    placement.x = 100;
}, () => { sum++; });
if (sum !== 49) throw new Error(`slot result = ${sum}`);
if (Caller_OwnContent(null) !== 49 || Caller_ImportedContent(null) !== 49)
    throw new Error("local and imported function values must bind the same slot");
''')
                run("node", str(driver))

        provider.write_text(PROVIDER)
        caller.write_text(CALLER)
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
        invalid_blocks = {
            "missing": (PROVIDER, CALLER.replace("        finish = Finish\n", ""), "missing required widget slot"),
            "duplicate": (PROVIDER, CALLER.replace("        finish = Finish", "        finish = Finish\n        finish = Finish"), "duplicate widget property or slot"),
            "unknown": (PROVIDER, CALLER.replace("        first = Next()", "        missing = Next()"), "unknown widget property or slot"),
            "slot type": (PROVIDER, CALLER.replace("        finish = Finish", "        finish = Accumulate"), "function does not match slot signature"),
            "parameter type": (PROVIDER.replace("finish: Empty) #ui", "finish: i32) #ui"), CALLER, "widget parameters after props must be typed slots"),
            "name conflict": (PROVIDER.replace("    second: i32", "    content: i32\n    second: i32"), CALLER, "widget slot name conflicts"),
        }
        for name, (source, consumer, diagnostic) in invalid_blocks.items():
            provider.write_text(source)
            caller.write_text(consumer)
            for strict in ([], ["--strict"]):
                result = subprocess.run([*command, *strict, str(caller), str(provider)],
                                        text=True, capture_output=True)
                if result.returncode == 0 or diagnostic not in result.stderr:
                    raise AssertionError(f"{target}: block {name}: {result.stderr}")
        provider.write_text(PROVIDER)
        caller.write_text(CALLER)
        invalid_captures = {
            "unknown capture": (CALLER.replace("sum += placement.x * index + record.x", "missing += index"), "unresolved name"),
            "captured type": (CALLER.replace("sum += placement.x * index + record.x", "sum = true"), "assignment type mismatch"),
            "slot escape": (CALLER + "EscapeBody :: () {\n    outer: Empty = Finish\n    if true {\n        inner: Empty = () #slot {\n            return\n        }\n        outer = inner\n    }\n}\n", "slot assignment cannot escape its lexical block"),
            "capture slot write": (CALLER + "EscapeCapture :: () {\n    outer: Empty = Finish\n    child: Empty = () #slot {\n        outer = Finish\n    }\n    child()\n}\n", "slot assignment cannot escape its lexical block"),
            "slot result": (CALLER.replace("            sum += 3", "            return 3"), "return type mismatch"),
        }
        for name, (consumer, diagnostic) in invalid_captures.items():
            caller.write_text(consumer)
            for strict in ([], ["--strict"]):
                result = subprocess.run([*command, *strict, str(caller), str(provider)],
                                        text=True, capture_output=True)
                if result.returncode == 0 or diagnostic not in result.stderr:
                    raise AssertionError(f"{target}: capture {name}: {result.stderr}")
        caller.write_text(CALLER)
        invalid_values = {
            "return": (PROVIDER.replace("Accumulate :: (placement: Placement, index: i32)",
                                        "Accumulate :: (placement: Placement, index: i32) -> i32"), CALLER),
            "arity": (PROVIDER.replace("Accumulate :: (placement: Placement, index: i32)",
                                       "Accumulate :: (placement: Placement)"), CALLER),
            "type": (PROVIDER.replace("Accumulate :: (placement: Placement, index: i32)",
                                      "Accumulate :: (placement: Placement, index: i64)"), CALLER),
            "unknown": (PROVIDER, CALLER.replace("child = Accumulate", "child = Missing")),
        }
        for name, (source, consumer) in invalid_values.items():
            provider.write_text(source)
            caller.write_text(consumer)
            for strict in ([], ["--strict"]):
                result = subprocess.run([*command, *strict, str(caller), str(provider)],
                                        text=True, capture_output=True)
                if result.returncode == 0 or "function does not match slot signature" not in result.stderr:
                    raise AssertionError(f"{target}: function value {name}: {result.stderr}")
        provider.write_text(PROVIDER)
        caller.write_text(CALLER)
        print(f"{target}: typed slot invocation, forwarding, copies, and diagnostics passed")

    # Runtime functions must bind the receiver that created the slot, including
    # when its host dependency exists only through a function value.
    source = work / "values.kry"
    source.write_text('''#module "values"
Content :: (value: i32) #slot
SetValue :: (value: i32) #extern
Record :: (value: i32) {
    SetValue(value)
}
Invoke :: (Button: Content) {
    Button(17)
}
Run :: () {
    increment: i32 = 0
    child: Content = (instance_host_0: i32) #slot {
        SetValue(instance_host_0 + increment)
        increment += 1
    }
    Invoke(child)
    if increment != 1 { SetValue(-100) }
}
''')
    output = work / "native"
    run(str(BUILD / "bin/k2go"), "--strict", "--no-main", "--runtime-implementation",
        "--pkg", "slots", "--root", str(work), "-o", str(output), str(source))
    driver = output / "values_test.go"
    driver.write_text('''package slots
import "testing"
type runtime struct { value int32 }
func (r *runtime) SetValue(value int32) { r.value += value }
func TestSlotReceiver(t *testing.T) {
    first, second := &runtime{}, &runtime{}
    first.Values_Run()
    first.Values_Run()
    second.Values_Run()
    if first.value != 34 || second.value != 17 {
        t.Fatal("slot function lost its runtime receiver", first.value, second.value)
    }
}
''')
    run("gofmt", "-w", *map(str, output.glob("*.go")))
    run("go", "test", *map(str, output.glob("*.go")))
    print("native Go: function values preserve their originating runtime receiver")
