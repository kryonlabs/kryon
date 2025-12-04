# Reactive Primitives POC - Quick Start Guide

## What is This?

This POC demonstrates **universal reactive primitives** in .kir v2.1 format, enabling bidirectional code generation between Nim, Lua, and JavaScript.

## Quick Demo

### 1. Build the POC Test

```bash
cd /home/wao/Projects/kryon
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH nim c -o:bin/test_reactive_poc tests/poc_reactive/test_reactive_poc.nim
```

### 2. Generate .kir v2.1 File

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./bin/test_reactive_poc
```

Output: `/tmp/test_reactive_poc.kir` (contains UI + reactive manifest)

### 3. Inspect the .kir File

```bash
# View the entire file
cat /tmp/test_reactive_poc.kir | jq .

# View just the reactive manifest
jq .reactive_manifest /tmp/test_reactive_poc.kir
```

### 4. Generate Lua Code

```bash
./bin/cli/kryon codegen /tmp/test_reactive_poc.kir --lang=lua
```

Or save to file:
```bash
./bin/cli/kryon codegen /tmp/test_reactive_poc.kir --lang=lua --output=/tmp/output.lua
```

### 5. Generate Nim Code

```bash
./bin/cli/kryon codegen /tmp/test_reactive_poc.kir --lang=nim
```

Or save to file:
```bash
./bin/cli/kryon codegen /tmp/test_reactive_poc.kir --lang=nim --output=/tmp/output.nim
```

## .kir v2.1 Format Structure

```json
{
  "format_version": "2.1",
  "component": {
    "id": 0,
    "type": "Row",
    "children": [...]
  },
  "reactive_manifest": {
    "format_version": "2.1",
    "variables": [
      {
        "id": 1,
        "name": "counter",
        "type": "int",
        "initial_value": "0",
        "scope": "global"
      }
    ],
    "bindings": [],
    "conditionals": [],
    "for_loops": []
  }
}
```

## Code Generation Examples

### Lua Output

```lua
local Reactive = require("kryon.reactive")

-- Reactive State
local counter = Reactive.state(0)
local message = Reactive.state("Hello POC!")

-- UI Component Tree
return kryon.Container({...})
```

### Nim Output

```nim
import ../../bindings/nim/reactive_system

# Reactive State
var counter = namedReactiveVar("counter", 0)
var message = namedReactiveVar("message", "Hello POC!")

proc main() =
  var root = Container:
    ...
```

## Supported Reactive Types

- `int` - Integer values
- `float` - Floating-point values
- `string` - String values
- `bool` - Boolean values

## CLI Commands

```bash
# Generate Lua code
kryon codegen <file.kir> --lang=lua [--output=<file.lua>]

# Generate Nim code
kryon codegen <file.kir> --lang=nim [--output=<file.nim>]

# Inspect .kir structure
jq . <file.kir>

# View reactive manifest only
jq .reactive_manifest <file.kir>
```

## Creating Your Own Reactive Apps

```nim
import ../../bindings/nim/kryon_dsl/impl
import ../../bindings/nim/runtime
import ../../bindings/nim/reactive_system
import ../../bindings/nim/ir_serialization

# 1. Create reactive variables
var count = namedReactiveVar("count", 0)
var name = namedReactiveVar("name", "World")

proc main() =
  # 2. Build UI with reactive bindings
  var root = Container:
    Text:
      content = fmt"Count: {count.value}"

    Button:
      title = "Increment"
      onClick = proc() = count.value += 1

  # 3. Export reactive manifest
  let manifest = exportReactiveManifest()

  # 4. Serialize to .kir v2.1
  if ir_write_json_v2_with_manifest_file(root, manifest, "my_app.kir"):
    echo "✓ Saved my_app.kir"

  # 5. Cleanup
  ir_reactive_manifest_destroy(manifest)
  ir_destroy_component(root)

main()
```

## Files in This POC

- `test_reactive_poc.nim` - Example app demonstrating reactive state
- `REACTIVE_POC_SUMMARY.md` - Detailed implementation documentation
- `README.md` - This file

## See Also

- Full implementation details: [REACTIVE_POC_SUMMARY.md](REACTIVE_POC_SUMMARY.md)
- Kryon CLI documentation: `kryon --help`
- .kir format specification: `/home/wao/Projects/kryon/docs/KIR_FORMAT_V2.md`
