#!/usr/bin/env python3
"""Compile and execute typed stateless widget blocks through all four targets."""

import os
import re
from itertools import product
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / (sys.argv[1] if len(sys.argv) > 1 else "build/linux-x86_64")).resolve()
BIN = BUILD / "bin"


def run(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"{args}\n{result.stdout}\n{result.stderr}")
    return result


PROVIDER = '''#module "cards"
CardProps :: struct {
    value: i32
}
state {
    sum: i32 = 0
}
Card :: (props: CardProps) #ui {
    sum += props.value
    props.value = 99
}
Reset :: () {
    sum = 0
}
Read :: () -> i32 {
    return sum
}
'''

CALLER = '''#module "consumer"
#import "cards"
Run :: () -> i32 {
    Reset()
    Card first: {
        value = 3
    }
    Card second: {
        value = 4
    }
    Card empty: {
    }
    props: CardProps
    props.value = 5
    Card(props)
    return Read() + props.value
}
'''

LOCAL = '''LocalProps :: struct {
    value: i32
}
Local :: (props: LocalProps) #ui {
    forwarded: CardProps
    forwarded.value = props.value
    Card(forwarded)
}
'''


with tempfile.TemporaryDirectory(prefix="kryon-widget-declarations-") as directory:
    work = Path(directory)
    provider = work / "cards.kry"
    caller = work / "consumer.kry"
    provider.write_text(PROVIDER)
    caller.write_text(CALLER)
    for target in ("c", "cpp", "go", "js"):
        output = work / target
        flags = ["--runtime", "./kryon-runtime.js"] if target == "js" else []
        command = [str(BIN / f"k2{target}"), "--no-main", *flags,
                   "--root", str(work), "-o", str(output)]
        local_caller = CALLER.replace("Card first:", "Local first:")
        switch_caller = CALLER.replace("    Card(props)", '''    switch 1 {
        case 1: {
            Card(props)
            break
        }
        default: {
            Card(props)
            break
        }
    }''')
        cases = (CALLER, local_caller + LOCAL,
                 local_caller.replace('#import "cards"', '#import "cards"\n' + LOCAL),
                 switch_caller, switch_caller.replace("switch 1", "switch 2"))
        returning_provider = PROVIDER.replace("Card :: (props: CardProps) #ui", "Card :: (props: CardProps) -> bool #ui")
        returning_provider = returning_provider.replace("    props.value = 99", "    activated: bool = props.value > 0\n    props.value = 99\n    return activated")
        returning_caller = CALLER.replace("    Card(props)\n    return Read() + props.value", '''    if Card(props) {
        return Read() + props.value
    }
    return -1''')
        returning_caller = returning_caller.replace("    props.value = 5", '''    inactive: CardProps
    if Card(inactive) {
        return -1
    }
    props.value = 5''')
        declarations = [(source, PROVIDER) for source in cases]
        portable_ui_provider = '''#module "cards"
CardProps :: struct {
    value: i8
}
state {
    result: i32 = 0
}
Card :: (props: CardProps) #ui {
    props.value += 1
    if props.value == -128 {
        result = 17
    }
}
Read :: () -> i32 {
    return result
}
'''
        portable_ui_caller = '''#module "consumer"
#import "cards"
Run :: () -> i32 {
    Card boundary: {
        value = 127
    }
    return Read()
}
'''
        declarations.append((portable_ui_caller, portable_ui_provider))
        declarations.append((portable_ui_caller.replace('''    Card boundary: {
        value = 127
    }''', '    Card((CardProps){.value = 127})'), portable_ui_provider))
        ordered_provider = '''#module "cards"
CardProps :: struct {
    first: i32
    second: i32
}
state {
    sequence: i32 = 1
    result: i32 = -1
}
Next :: () -> i32 {
    value: i32 = sequence
    sequence += 1
    return value
}
Card :: (props: CardProps) #ui {
    if props.first == 1 && props.second == 2 && sequence == 3 {
        result = 17
    }
}
Read :: () -> i32 {
    return result
}
'''
        ordered_caller = '''#module "consumer"
#import "cards"
Run :: () -> i32 {
    Card ordered: {
        first = Next()
        second = Next()
    }
    return Read()
}
'''
        declarations.extend(((ordered_caller, ordered_provider),
            (ordered_caller.replace('    Card ordered:', '''    switch 1 {
        case 1: {
            break
        }
        default: {
            break
        }
    }
    Card ordered:'''), ordered_provider),
            (ordered_caller.replace('''    Card ordered: {
        first = Next()
        second = Next()
    }''', '    Card((CardProps){.first = Next(), .second = Next()})'), ordered_provider)))
        # Constructors must work at widget boundaries, including forwarding
        # imported props through a locally declared widget.
        literal_caller = CALLER.replace(
            "    props: CardProps\n    props.value = 5",
            "    props: CardProps = (CardProps){.value = 5}")
        literal_local = LOCAL.replace(
            "    forwarded: CardProps\n    forwarded.value = props.value",
            "    forwarded: CardProps = (CardProps){.value = props.value}")
        declarations.extend((source, PROVIDER) for source in (
            literal_caller,
            literal_caller.replace("Card first:", "Local first:") + literal_local,
            literal_caller.replace("    Card(props)",
                                   "    Card((CardProps){.value = props.value})")))
        declarations.extend((source, returning_provider) for source in (*cases, returning_caller))
        named_action = '''
Button :: (props: CardProps) -> bool #ui {
    return props.value > 0
}
'''
        named_caller = CALLER.replace("    Card(props)", '''    inactive: CardProps
    if Button(inactive) {
        return -1
    } else if Button(props) {
        Card(props)
    } else {
        return -1
    }''')
        declarations.extend(((named_caller, PROVIDER + named_action),
                             (named_caller + named_action, PROVIDER)))
        # A widget may compose a shared style record into its own props.
        # Mutating either level locally must not mutate the caller's value.
        styled_provider = PROVIDER.replace("CardProps :: struct {", '''Appearance :: struct {
    inset: i32
}
CardProps :: struct {
    appearance: Appearance''').replace(
            "    props.value = 99", "    props.value = 99\n    props.appearance.inset = 99")
        styled_caller = CALLER.replace(
            "    props.value = 5", "    props.value = 5\n    props.appearance.inset = 7").replace(
            "    return Read() + props.value", '''    if props.appearance.inset != 7 {
        return -1
    }
    return Read() + props.value''')
        declarations.append((styled_caller, styled_provider))
        declarations.append((styled_caller.replace('    Card(props)', '''    switch 1 {
        case 1: {
            Card(props)
            break
        }
        default: {
            break
        }
    }'''), styled_provider))
        declarations.append((styled_caller.replace(
            "    props: CardProps\n    props.value = 5\n    props.appearance.inset = 7",
            "    props: CardProps = (CardProps){.value = 5, "
            ".appearance = (Appearance){.inset = 7}}"), styled_provider))
        enum_provider = PROVIDER + '''
Inset :: enum {
    InsetNone = 0
    InsetCompact = 3
    InsetRegular
    InsetWide
}
'''
        enum_caller = CALLER.replace("value = 3", "value = InsetCompact").replace(
            "value = 4", "value = InsetRegular").replace(
            "props.value = 5", "props.value = (i32)InsetWide")
        declarations.append((enum_caller, enum_provider))
        # Built-in spelling must not hide an explicitly declared leaf widget.
        for name in ("Button", "Text"):
            declared_caller = returning_caller.replace("Card ", name + " ").replace("Card(", name + "(")
            declared_provider = returning_provider.replace("Card ::", name + " ::")
            declarations.append((declared_caller, declared_provider))
            local_declaration = f'''\n{name} :: (props: CardProps) -> bool #ui {{
    Card(props)
    return props.value > 0
}}
'''
            declarations.append((declared_caller + local_declaration, PROVIDER))
        for (source, declaration), sources in product(declarations, ((caller, provider), (provider, caller))):
            caller.write_text(source)
            provider.write_text(declaration)
            portable_flags = ["--strict"] if declaration == portable_ui_provider else []
            run(*command, *portable_flags, *(str(path) for path in sources))
            if target in ("c", "cpp"):
                (output / "ui_inspect.h").write_text('''
static inline void PushUIInspectSource(const char *p, int n) {(void)p; (void)n;}
static inline void PopUIInspectSource(void) {}
''')
                header = "h" if target == "c" else "hpp"
                driver = output / f"driver.{target}"
                driver.write_text(f'#include "consumer.{header}"\nint main(void) {{ return consumer_Run() != 17; }}\n')
                compiler = os.environ.get("CC", "cc") if target == "c" else os.environ.get("CXX", "c++")
                run(compiler, str(driver), str(output / f"consumer.{target}"),
                    str(output / f"cards.{target}"), "-o", str(output / "test"))
                run(str(output / "test"))
            elif target == "go":
                driver = output / "declarations_test.go"
                driver.write_text('''package krygen
import "testing"
func TestDeclarations(t *testing.T) {
    if Consumer_Run() != 17 { t.Fatal("block and function invocations disagree") }
}
''')
                run("go", "test", str(output / "consumer.go"), str(output / "cards.go"), str(driver))
            else:
                for runtime_file in (ROOT / "web").glob("*.js"):
                    shutil.copyfile(runtime_file, output / runtime_file.name)
                (output / "package.json").write_text('{"type":"module"}\n')
                driver = output / "test.mjs"
                driver.write_text('''import { Consumer_Run } from "./consumer.js";
const actual = Consumer_Run(null);
if (actual !== 17) throw new Error(`block and function invocations disagree: ${actual}`);
''')
                try:
                    run("node", str(driver))
                except AssertionError as failure:
                    raise AssertionError(f"{source}\n{declaration}\n{failure}") from failure

        provider.write_text(PROVIDER)
        invalid = {
            "unknown": (CALLER.replace("Card first:", "Missing first:"), "unknown widget declaration"),
            "field": (CALLER.replace("value = 3", "missing = 3"), "unknown initializer field"),
            "type": (CALLER.replace("value = 3", "value = true"), "type mismatch"),
            "duplicate": (CALLER.replace("value = 3", "value = 3\n        value = 4"), "duplicate initializer field"),
            "not_ui": (CALLER.replace("Card first:", "Read first:"), "requires a #ui declaration"),
            "children": (CALLER.replace("value = 3", "value = 3\n        Read()"), "do not yet accept child content"),
            "call_type": (CALLER.replace("Card(props)", "Card(3)"), "argument type mismatch"),
            "call_missing": (CALLER.replace("Card(props)", "Card()"), "argument count mismatch"),
            "call_extra": (CALLER.replace("Card(props)", "Card(props, props)"), "argument count mismatch"),
            "local_call_type": (CALLER.replace("Card(props)", "Local(3)") + LOCAL, "argument type mismatch"),
        }
        for name, (source, diagnostic) in invalid.items():
            caller.write_text(source)
            result = subprocess.run([*command, str(caller), str(provider)], text=True, capture_output=True)
            assert result.returncode != 0, (target, name, result.stdout)
            assert diagnostic in result.stderr, (target, name, result.stderr)
        for widget in ("Button", "Text"):
            declaration = returning_provider.replace("Card ::", widget + " ::")
            source = CALLER.replace("Card ", widget + " ").replace("Card(", widget + "(")
            for invalid_source, invalid_declaration, diagnostic in (
                (source.replace("value = 3", "missing = 3"), declaration, "unknown initializer field"),
                (source.replace("value = 3", "value = true"), declaration, "type mismatch"),
                (source.replace("value = 3", "value = 3\n        value = 4"), declaration, "duplicate initializer field"),
                (source, declaration.replace(" #ui", ""), "requires a #ui declaration"),
            ):
                caller.write_text(invalid_source)
                provider.write_text(invalid_declaration)
                result = subprocess.run([*command, str(caller), str(provider)], text=True, capture_output=True)
                assert result.returncode != 0, (target, widget, "invalid declaration fell back to host widget")
                assert diagnostic in result.stderr, (target, widget, result.stderr)
        provider.write_text(PROVIDER)
        caller.write_text(CALLER)

        # Two direct imports must not silently select different style values
        # depending on backend or source order.
        other = work / "other.kry"
        other.write_text('''#module "other"
OtherInset :: enum {
    InsetWide = 9
}
''')
        provider.write_text(enum_provider)
        caller.write_text(enum_caller.replace('#import "cards"', '#import "cards"\n#import "other"'))
        for sources in ((caller, provider, other), (other, provider, caller)):
            result = subprocess.run([*command, "--strict", *(str(path) for path in sources)],
                                    text=True, capture_output=True)
            assert result.returncode != 0, (target, "ambiguous enum member accepted")
            assert "ambiguous enum member" in result.stderr, (target, result.stderr)
        provider.write_text(PROVIDER)
        caller.write_text(CALLER)

        # Large style/content values must survive declaration lowering intact.
        # Aggregate limits must be diagnosed, never silently drop a property.
        large = work / "large.kry"
        prefix = '''#module "large"
Props :: struct {
    first: const char*
    second: const char*
}
Card :: (props: Props) #ui {
}
Value :: (a: const char*, b: const char*, c: const char*, d: const char*) -> const char* {
    return d
}
Render :: () #ui {
    Card sample: {
'''
        suffix = '''    }
}
'''
        values = [character * 780 + "END_OF_PROPERTY" for character in "wxyz"]
        value = "Value(\n" + ",\n".join(f'"{part}"' for part in values) + "\n)"
        large.write_text(prefix + f'        first = {value}\n' + suffix)
        run(*command, str(large))
        generated = (output / f"large.{target}").read_text()
        assert all(part in generated for part in values), (target, "truncated widget property")
        large.write_text(prefix + f'        first = {value}\n'
                         + f'        second = {value}\n' + suffix)
        result = subprocess.run([*command, str(large)], text=True, capture_output=True)
        assert result.returncode != 0, (target, "silently dropped oversized widget property")
        assert "widget properties exceed" in result.stderr, (target, result.stderr)

        # Built-in blocks must not have a separate, lower truncation threshold.
        values = [character * 950 + "END_OF_PROPERTY" for character in "wxyz"]
        value = "Value(\n" + ",\n".join(f'"{part}"' for part in values) + "\n)"
        builtin_prefix = '#import "kryon.h"\n' + prefix.replace("Card sample:", "Button sample:")
        large.write_text(builtin_prefix + f'        label = {value}\n' + suffix)
        run(*command, str(large))
        generated = (output / f"large.{target}").read_text()
        assert all(part in generated for part in values), (target, "truncated built-in widget property")

        # Property values mentioning .key must not suppress a layout's identity.
        identity = work / "identity.kry"
        identity.write_text('''#import "kryon.h"
Settings :: struct {
    key_padding: int
    KeyPadding: int
}
Layout :: (settings: Settings) #ui {
    Column automatic: {
        padding = settings.key_padding
    }
    Row capitalized: {
        padding = settings.KeyPadding
    }
    Column explicit: {
        padding = settings.key_padding
        key = Key("chosen")
    }
    Button custom: {
        label = "Flat"
        style = (ControlStyle){.normal = {.fields = StyleMaterial, .material = MaterialFlat}, .hover = {.fields = StyleMaterial, .material = MaterialLightfield}}
    }
    Button composed: {
        bounds = {0, 0, 100, 40}
        Text((TextProps){.text="Child"})
    }
}
''')
        run(*command, str(identity))
        generated = (output / f"identity.{target}").read_text()
        # JavaScript's diagnostic widget arguments are escaped string data.
        assert "Layout/automatic" in generated, (target, "lost automatic layout key")
        assert "Layout/capitalized" in generated, (target, "lost key after capitalized member access")
        assert "Layout/explicit" not in generated, (target, "replaced explicit key")
        assert "chosen" in generated, (target, "lost explicit layout key")
        if target == "js":
            assert generated.count('"Button"') == 1, (target, "leaf button opened a content scope")
            assert generated.count('"BeginButton"') == 1, (target, "composed button lost its content scope")
        else:
            assert len(re.findall(r"(?<![A-Za-z])Button\(", generated)) == 1, (target, "leaf button opened a content scope")
            assert generated.count("BeginButton(") == 1, (target, "composed button lost its content scope")

print("widget declarations: typed blocks and ordinary calls agree in C, C++, Go, JavaScript")
