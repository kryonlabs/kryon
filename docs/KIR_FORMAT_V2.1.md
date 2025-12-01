# Kryon IR Format v2.1 - With Bytecode Support

**Version:** 2.1
**Date:** 2025-12-01
**Status:** DRAFT

## Overview

Kryon IR v2.1 extends v2.0 with complete event logic serialization through an embedded bytecode VM. This enables true frontend-independence where .kir files contain all UI logic and can run on any backend without the original source code.

## Architecture: Hybrid "Universal Core + Host Bindings"

### Layer 1: Universal Core (Embedded Bytecode)
Simple UI logic that runs everywhere:
- Button clicks, hover states, menu toggling
- Counter increments, text updates, form validation
- Simple arithmetic, string operations
- State management (numbers, strings, booleans)

### Layer 2: Host Extensions (Native FFI)
Complex operations requiring native capabilities:
- File I/O, Networking, Database access
- Heavy computation (FFT, crypto, physics)
- System APIs, Hardware access

## File Structure

```json
{
  "version": "2.1",
  "component": { /* Root component tree */ },
  "functions": [ /* Bytecode functions */ ],
  "states": [ /* Reactive state definitions */ ],
  "host_functions": [ /* Host function registry */ ]
}
```

## Section 1: Component Tree (Unchanged)

The component tree structure remains identical to v2.0:

```json
{
  "component": {
    "id": 1,
    "type": "Column",
    "width": "800.0px",
    "height": "600.0px",
    "children": [
      {
        "id": 2,
        "type": "Button",
        "text": "Increment",
        "onClick": {
          "function_id": 1
        }
      }
    ]
  }
}
```

**Key Changes:**
- Event handlers (onClick, onChange, etc.) now reference `function_id` instead of being empty
- Component structure and styling remain identical to v2.0

## Section 2: Functions Array (NEW)

Defines bytecode functions that can be called by event handlers.

```json
{
  "functions": [
    {
      "id": 1,
      "name": "handle_increment",
      "bytecode": [
        {"op": "GET_STATE", "arg": 1},
        {"op": "PUSH_INT", "arg": 1},
        {"op": "ADD"},
        {"op": "SET_STATE", "arg": 1},
        {"op": "HALT"}
      ]
    }
  ]
}
```

### Function Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | integer | Yes | Unique function identifier |
| `name` | string | Yes | Function name (for debugging) |
| `bytecode` | array | Yes | Array of bytecode instructions |

### Bytecode Instruction Format

Each instruction is an object with:

```json
{
  "op": "OPCODE_NAME",
  "arg": <value>  // Optional, depends on opcode
}
```

**Opcodes:**

| Opcode | Arg Type | Description |
|--------|----------|-------------|
| **Stack Operations** |
| PUSH_INT | integer | Push integer onto stack |
| PUSH_FLOAT | float | Push float onto stack |
| PUSH_STRING | string | Push string onto stack |
| PUSH_BOOL | boolean | Push boolean onto stack |
| POP | - | Pop top value |
| DUP | - | Duplicate top of stack |
| **Arithmetic** |
| ADD | - | Pop 2 values, push sum |
| SUB | - | Pop 2 values, push difference |
| MUL | - | Pop 2 values, push product |
| DIV | - | Pop 2 values, push quotient |
| MOD | - | Pop 2 values, push remainder |
| NEG | - | Negate top of stack |
| **Comparison** |
| EQ | - | Pop 2 values, push equality |
| NE | - | Pop 2 values, push inequality |
| LT | - | Pop 2 values, push less-than |
| GT | - | Pop 2 values, push greater-than |
| LE | - | Pop 2 values, push less-equal |
| GE | - | Pop 2 values, push greater-equal |
| **Logical** |
| AND | - | Pop 2 bools, push AND |
| OR | - | Pop 2 bools, push OR |
| NOT | - | Pop bool, push NOT |
| **String** |
| CONCAT | - | Pop 2 strings, push concatenation |
| **State Management** |
| GET_STATE | state_id | Push state value onto stack |
| SET_STATE | state_id | Pop value, store in state |
| GET_LOCAL | local_id | Push local variable |
| SET_LOCAL | local_id | Pop value, store in local |
| **Control Flow** |
| JUMP | offset | Unconditional jump |
| JUMP_IF_FALSE | offset | Jump if top of stack is false |
| CALL | function_id | Call internal function |
| RETURN | - | Return from function |
| **Host Interaction** |
| CALL_HOST | function_id | Call host-provided function |
| GET_PROP | component_id, prop | Get component property |
| SET_PROP | component_id, prop | Set component property |
| **System** |
| HALT | - | Stop execution |

## Section 3: States Array (NEW)

Defines reactive states that can be accessed via GET_STATE/SET_STATE.

```json
{
  "states": [
    {
      "id": 1,
      "name": "counter",
      "type": "int",
      "initial_value": 0
    },
    {
      "id": 2,
      "name": "username",
      "type": "string",
      "initial_value": ""
    }
  ]
}
```

### State Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | integer | Yes | Unique state identifier |
| `name` | string | Yes | State name (for debugging) |
| `type` | string | Yes | "int", "float", "string", "bool" |
| `initial_value` | any | Yes | Initial value matching type |

## Section 4: Host Functions Array (NEW)

Defines host functions that must be provided by the backend.

```json
{
  "host_functions": [
    {
      "id": 100,
      "name": "save_to_file",
      "signature": "(string) -> bool",
      "required": false
    }
  ]
}
```

### Host Function Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | integer | Yes | Unique host function identifier |
| `name` | string | Yes | Host function name |
| `signature` | string | Yes | Function signature (for documentation) |
| `required` | boolean | No | If true, app fails if not available. Default: false |

**Graceful Degradation:** If `required` is false and the host function is not available, the VM continues execution and logs a warning.

## Complete Example

```json
{
  "version": "2.1",
  "component": {
    "id": 1,
    "type": "Column",
    "width": "800.0px",
    "height": "600.0px",
    "background": "#f0f0f0",
    "children": [
      {
        "id": 2,
        "type": "Column",
        "padding": 30,
        "alignItems": "stretch",
        "children": [
          {
            "id": 3,
            "type": "Text",
            "text": "Counter App",
            "fontSize": 28,
            "color": "#000000"
          },
          {
            "id": 4,
            "type": "Row",
            "alignItems": "center",
            "children": [
              {
                "id": 5,
                "type": "Text",
                "text": "Count: 0",
                "fontSize": 18,
                "color": "#000000",
                "reactive_text": {
                  "state_id": 1,
                  "format": "Count: {}"
                }
              },
              {
                "id": 6,
                "type": "Button",
                "text": "+1",
                "width": "80.0px",
                "height": "40.0px",
                "onClick": {
                  "function_id": 1
                }
              },
              {
                "id": 7,
                "type": "Button",
                "text": "Save",
                "width": "80.0px",
                "height": "40.0px",
                "onClick": {
                  "function_id": 2
                }
              }
            ]
          }
        ]
      }
    ]
  },
  "functions": [
    {
      "id": 1,
      "name": "handle_increment",
      "bytecode": [
        {"op": "GET_STATE", "arg": 1},
        {"op": "PUSH_INT", "arg": 1},
        {"op": "ADD"},
        {"op": "SET_STATE", "arg": 1},
        {"op": "HALT"}
      ]
    },
    {
      "id": 2,
      "name": "handle_save",
      "bytecode": [
        {"op": "GET_STATE", "arg": 1},
        {"op": "CALL_HOST", "arg": 100},
        {"op": "HALT"}
      ]
    }
  ],
  "states": [
    {
      "id": 1,
      "name": "counter",
      "type": "int",
      "initial_value": 0
    }
  ],
  "host_functions": [
    {
      "id": 100,
      "name": "save_counter_to_file",
      "signature": "(int) -> void",
      "required": false
    }
  ]
}
```

## Bytecode Execution Model

### Event Handler Flow

1. User clicks button (component ID 6)
2. Backend looks up onClick handler → function_id: 1
3. Backend creates VM instance
4. Backend initializes states from `states` array
5. Backend registers host functions from `host_functions` array
6. VM executes bytecode from function ID 1
7. VM updates states via SET_STATE
8. Backend observes state changes and updates UI

### VM State Isolation

Each function execution gets:
- Shared reactive state (persistent across calls)
- Fresh local variable table (reset each call)
- Shared host function registry

### Host Function Integration

```c
// Backend registers host function
ir_vm_register_host_function(vm, 100, "save_counter_to_file",
                              my_save_handler, NULL);

// When bytecode executes CALL_HOST 100:
void my_save_handler(IRVM* vm, void* user_data) {
    IRValue counter_value;
    ir_vm_pop(vm, &counter_value);  // Pop argument

    if (counter_value.type == VAL_INT) {
        save_to_file(counter_value.as.i);
    }
}
```

## Binary Format (.kirb)

The binary format remains unchanged - bytecode is encoded as:

```
OPCODE (1 byte) + OPERAND (variable length)
```

Example:
- PUSH_INT 42 → `0x01 0x2A 0x00 0x00 0x00 0x00 0x00 0x00 0x00`
- ADD → `0x10`
- GET_STATE 1 → `0x50 0x01 0x00 0x00 0x00`

## Backwards Compatibility

.kir v2.1 files are **backwards compatible** with v2.0 readers:
- If `functions`, `states`, or `host_functions` arrays are missing, treat as empty
- Components without onClick handlers work as static UI
- v2.0 readers ignore unknown fields

.kir v2.0 files are **forward compatible** with v2.1 readers:
- Missing bytecode sections → no event handlers
- Static UI still renders correctly

## Validation Rules

1. All function IDs referenced in event handlers must exist in `functions` array
2. All state IDs in bytecode must exist in `states` array
3. All host function IDs in bytecode must exist in `host_functions` array
4. Bytecode must be well-formed (valid opcodes, correct operand types)
5. State initial_value must match declared type

## Tooling

### CLI Commands

```bash
# Compile Nim to .kir v2.1 with bytecode
kryon compile app.nim --format=json --bytecode

# Validate .kir file including bytecode
kryon validate app.kir --check-bytecode

# Inspect bytecode functions
kryon inspect app.kir --show-bytecode

# Disassemble bytecode to human-readable form
kryon disassemble app.kir
```

### Disassembly Output

```
Function 1: handle_increment
  0000: GET_STATE 1      ; Load counter
  0005: PUSH_INT 1       ; Push constant 1
  000E: ADD              ; counter + 1
  000F: SET_STATE 1      ; Save counter
  0014: HALT             ; Done
```

## Migration Guide (v2.0 → v2.1)

1. Add empty `functions`, `states`, `host_functions` arrays to existing .kir files
2. Convert onClick handlers from empty objects to `{"function_id": N}`
3. Create bytecode functions for each handler
4. Define reactive states used by handlers
5. Declare any host functions called by bytecode

## Future Extensions (v2.2+)

- `OP_CALL_ASYNC` for async host functions
- `OP_SPAWN` for concurrent execution
- `OP_AWAIT` for promise-like semantics
- Type annotations for better optimization
- JIT compilation hints
