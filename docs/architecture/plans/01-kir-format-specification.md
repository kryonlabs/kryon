# KIR Format Specification - Complete Design

## Executive Summary

This document defines the complete KIR (Kryon Intermediate Representation) format specification that serves as the universal intermediate language for the Kryon UI framework.

## Current State

### KIR Version 2.0 (Current)
Located in: `/mnt/storage/Projects/kryon/ir/ir_json_v2.c`

```c
#define IR_FORMAT_VERSION_MAJOR 2
#define IR_FORMAT_VERSION_MINOR 0
```

**Currently Serializes:**
- Component hierarchy (type, id, parent-child relationships)
- Style properties (background, color, fonts, borders)
- Layout properties (width, height, padding, margin, flexbox)
- Animations and transitions
- Pseudo-styles (:hover, :focus, :active)

**Missing:**
- Event handlers as source code
- Reactive state bindings
- Logic blocks (if/else, loops)
- Original source language metadata
- Component-level custom logic

### KIR Version 3.0 (Partial Implementation)

```c
char* ir_serialize_json_v3(IRComponent* root, IRReactiveManifest* manifest, IRLogicBlock* logic);
```

**Designed to include:**
- `IRReactiveManifest` - reactive state/variables
- `IRLogicBlock` - event handler logic as source code

**Status:** Function signature exists, partial implementation

## Problems with Current KIR

### 1. Event Handlers Not Preserved

**Example:** Button click handler lost in KIR

```tsx
// Source: button.tsx
<Button onClick={() => console.log("clicked")}>Click</Button>
```

```json
// Current KIR output (.kir file)
{
  "id": 1,
  "type": "Button",
  "text": "Click"
  // ❌ Event handler is LOST!
}
```

**Impact:** Cannot regenerate `.tsx` from KIR because logic is missing.

### 2. Source Language Not Tracked

**Problem:** KIR doesn't store whether original was C, Nim, Lua, or TSX

```json
{
  "id": 1,
  "type": "Container"
  // ❌ No "source_language": "tsx"
}
```

**Impact:**
- Cannot auto-select correct codegen
- Cannot preserve language-specific idioms
- Cannot do intelligent round-trip conversion

### 3. Reactive State Not Fully Represented

**Example from .kry:**

```kry
Container {
  @state count: int = 0

  Button {
    text: "Count: {count}"
    onClick: { count += 1 }
  }
}
```

**Current KIR:**
```json
{
  "type": "Container",
  "children": [{
    "type": "Button",
    "text": "Count: {count}"
    // ❌ State declaration lost
    // ❌ onClick handler lost
  }]
}
```

### 4. Logic Blocks Not Serialized

**C code example:**

```c
if (user->logged_in) {
    create_button("Logout");
} else {
    create_button("Login");
}
```

**Current KIR:**
```json
{
  "type": "Button"
  // ❌ Conditional logic lost
}
```

## Complete KIR Specification

### Core Principles

1. **Complete Preservation** - KIR must preserve 100% of semantic information
2. **Language Agnostic** - Works for C, Nim, Lua, TSX, HTML, etc.
3. **Round-Trip Capable** - Can regenerate source in any supported language
4. **Human Readable** - JSON format for debugging and inspection
5. **Metadata Rich** - Includes source language, file path, line numbers

### KIR JSON Structure

```json
{
  "format_version": "3.0",
  "metadata": {
    "source_language": "tsx|c|nim|lua|kry|html",
    "source_file": "path/to/source.tsx",
    "compiler_version": "kryon-1.0.0",
    "timestamp": "2025-01-27T12:00:00Z"
  },
  "reactive_manifest": {
    "states": [
      {
        "id": "state_1",
        "name": "count",
        "type": "int",
        "initial_value": "0",
        "source_code": "const [count, setCount] = useState(0)"
      }
    ],
    "computed": [
      {
        "id": "computed_1",
        "name": "doubleCount",
        "dependencies": ["state_1"],
        "expression": "count * 2"
      }
    ]
  },
  "logic_blocks": [
    {
      "id": "logic_1",
      "type": "event_handler",
      "event": "onClick",
      "component_id": 2,
      "language": "typescript",
      "source_code": "() => setCount(count + 1)",
      "dependencies": ["state_1"]
    },
    {
      "id": "logic_2",
      "type": "conditional",
      "condition": "user.logged_in",
      "language": "c",
      "source_code": "if (user->logged_in) { ... }"
    }
  ],
  "components": [
    {
      "id": 1,
      "type": "Container",
      "children": [2],
      "style": { ... }
    },
    {
      "id": 2,
      "type": "Button",
      "text": "Count: {state_1}",
      "events": {
        "onClick": "logic_1"
      },
      "style": { ... }
    }
  ]
}
```

### Field Specifications

#### metadata

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `source_language` | enum | Yes | Original source language (tsx, c, nim, lua, kry, html) |
| `source_file` | string | Yes | Path to original source file |
| `compiler_version` | string | Yes | Version of Kryon compiler used |
| `timestamp` | ISO8601 | No | When KIR was generated |
| `custom_metadata` | object | No | Language-specific metadata |

#### reactive_manifest

**States:**
| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique state identifier |
| `name` | string | Variable name |
| `type` | string | Data type (int, string, bool, object, etc.) |
| `initial_value` | string | Initial value as code |
| `source_code` | string | Original declaration code |

**Computed:**
| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique computed identifier |
| `name` | string | Computed property name |
| `dependencies` | string[] | IDs of states this depends on |
| `expression` | string | Computation expression |

#### logic_blocks

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique logic block ID |
| `type` | enum | event_handler, conditional, loop, function |
| `event` | string | Event name (if type=event_handler) |
| `component_id` | number | Associated component ID |
| `language` | string | Language of source_code |
| `source_code` | string | **Full source code of logic** |
| `dependencies` | string[] | IDs of states/props used |

#### components

*(Existing KIR 2.0 fields +)*

| Field | Type | Description |
|-------|------|-------------|
| `events` | object | Map of event_name → logic_id |
| `bindings` | object | Map of prop_name → state_id |
| `metadata` | object | Component-specific metadata |

## Implementation Plan

### Phase 1: Extend ir_core.h Structures

**File:** `/mnt/storage/Projects/kryon/ir/ir_core.h`

```c
// Add to IRComponent
typedef struct IRComponent {
    // ... existing fields ...

    // NEW in v3.0:
    char* source_language;              // "tsx", "c", "nim", etc.
    IREventBindings* event_bindings;    // Event → logic mapping
    IRPropertyBindings* property_bindings; // Prop → state mapping
} IRComponent;

// NEW: Event binding
typedef struct IREventBinding {
    char* event_name;          // "onClick", "onHover", etc.
    char* logic_block_id;      // Reference to IRLogicBlock
    struct IREventBinding* next;
} IREventBinding;

typedef struct IREventBindings {
    IREventBinding* bindings;
    size_t count;
} IREventBindings;

// NEW: Property binding to reactive state
typedef struct IRPropertyBinding {
    char* property_name;       // "text", "background", etc.
    char* state_id;           // Reference to reactive state
    char* expression;         // "Count: {count}"
    struct IRPropertyBinding* next;
} IRPropertyBinding;

typedef struct IRPropertyBindings {
    IRPropertyBinding* bindings;
    size_t count;
} IRPropertyBindings;
```

### Phase 2: Enhance Logic System

**File:** `/mnt/storage/Projects/kryon/ir/ir_logic.h`

```c
typedef enum {
    LOGIC_EVENT_HANDLER,
    LOGIC_CONDITIONAL,
    LOGIC_LOOP,
    LOGIC_FUNCTION,
    LOGIC_EXPRESSION
} IRLogicType;

typedef struct IRLogicBlock {
    char* id;                  // "logic_1", "logic_2", etc.
    IRLogicType type;
    char* event_name;          // For event handlers
    uint32_t component_id;     // Associated component
    char* language;            // Source code language
    char* source_code;         // **FULL SOURCE CODE**
    char** dependencies;       // IDs of states/props used
    size_t dependency_count;
    struct IRLogicBlock* next;
} IRLogicBlock;

// Container for all logic in the UI
typedef struct IRLogicManifest {
    IRLogicBlock* blocks;
    size_t count;
} IRLogicManifest;
```

### Phase 3: Enhance Reactive Manifest

**File:** `/mnt/storage/Projects/kryon/ir/ir_reactive_manifest.h`

```c
typedef struct IRReactiveState {
    char* id;              // "state_1"
    char* name;            // "count"
    char* type;            // "int", "string", etc.
    char* initial_value;   // "0", "\"hello\""
    char* source_code;     // Original declaration
    struct IRReactiveState* next;
} IRReactiveState;

typedef struct IRReactiveComputed {
    char* id;
    char* name;
    char** dependencies;   // state IDs
    size_t dependency_count;
    char* expression;      // Computation code
    struct IRReactiveComputed* next;
} IRReactiveComputed;

typedef struct IRReactiveManifest {
    IRReactiveState* states;
    size_t state_count;
    IRReactiveComputed* computed;
    size_t computed_count;
} IRReactiveManifest;
```

### Phase 4: Update JSON Serialization

**File:** `/mnt/storage/Projects/kryon/ir/ir_json_v2.c`

**Implement complete `ir_serialize_json_v3()`:**

```c
char* ir_serialize_json_v3(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicManifest* logic,
    IRSourceMetadata* metadata
) {
    cJSON* json = cJSON_CreateObject();

    // 1. Format version
    cJSON_AddStringToObject(json, "format_version", "3.0");

    // 2. Metadata section
    cJSON* meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "source_language", metadata->language);
    cJSON_AddStringToObject(meta, "source_file", metadata->file_path);
    // ... add all metadata
    cJSON_AddItemToObject(json, "metadata", meta);

    // 3. Reactive manifest
    if (manifest) {
        cJSON* reactive = serialize_reactive_manifest(manifest);
        cJSON_AddItemToObject(json, "reactive_manifest", reactive);
    }

    // 4. Logic blocks
    if (logic) {
        cJSON* logic_json = serialize_logic_manifest(logic);
        cJSON_AddItemToObject(json, "logic_blocks", logic_json);
    }

    // 5. Component tree
    cJSON* components = serialize_component_tree_v3(root);
    cJSON_AddItemToObject(json, "components", components);

    char* result = cJSON_Print(json);
    cJSON_Delete(json);
    return result;
}

static cJSON* serialize_logic_manifest(IRLogicManifest* logic) {
    cJSON* array = cJSON_CreateArray();

    IRLogicBlock* block = logic->blocks;
    while (block) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", block->id);
        cJSON_AddStringToObject(obj, "type", logic_type_to_string(block->type));
        if (block->event_name)
            cJSON_AddStringToObject(obj, "event", block->event_name);
        cJSON_AddNumberToObject(obj, "component_id", block->component_id);
        cJSON_AddStringToObject(obj, "language", block->language);

        // **KEY: Preserve source code**
        cJSON_AddStringToObject(obj, "source_code", block->source_code);

        // Dependencies
        if (block->dependency_count > 0) {
            cJSON* deps = cJSON_CreateArray();
            for (size_t i = 0; i < block->dependency_count; i++) {
                cJSON_AddItemToArray(deps, cJSON_CreateString(block->dependencies[i]));
            }
            cJSON_AddItemToObject(obj, "dependencies", deps);
        }

        cJSON_AddItemToArray(array, obj);
        block = block->next;
    }

    return array;
}
```

### Phase 5: Update Parsers to Preserve Logic

**TSX Parser:**
```c
// ir/parsers/tsx/ir_tsx_parser.c

// When parsing <Button onClick={() => ...}>
IRLogicBlock* logic = malloc(sizeof(IRLogicBlock));
logic->id = generate_logic_id();
logic->type = LOGIC_EVENT_HANDLER;
logic->event_name = "onClick";
logic->language = "typescript";
logic->source_code = strdup("() => setCount(count + 1)"); // Preserve!
```

**C Parser:**
```c
// ir/parsers/c/ir_c_parser.c

// When parsing event_bind(btn, "click", on_click_handler)
IRLogicBlock* logic = malloc(sizeof(IRLogicBlock));
logic->source_code = extract_function_body("on_click_handler"); // Get C source
```

**.kry Parser:**
```c
// ir/ir_kry_parser.c

// When parsing { count += 1 }
IRLogicBlock* logic = malloc(sizeof(IRLogicBlock));
logic->source_code = strdup(expr_token->value); // Already preserves source
```

## Success Criteria

✅ **Complete Preservation:**
- Event handlers stored as source code in KIR
- Reactive state declarations preserved
- Original language tracked

✅ **Round-Trip Capability:**
```bash
# TSX → KIR → TSX
kryon compile app.tsx -o app.kir
kryon codegen tsx app.kir -o app.generated.tsx
diff app.tsx app.generated.tsx  # Semantically equivalent

# C → KIR → Nim
kryon compile app.c -o app.kir
kryon codegen nim app.kir -o app.nim  # Translated with preserved logic
```

✅ **Multi-Language Support:**
- Same KIR can generate C, Nim, Lua, TSX, HTML with equivalent semantics
- Logic blocks translated between languages

## Files to Modify

```
ir/ir_core.h .................... Add event/property bindings
ir/ir_logic.h ................... Complete logic block system
ir/ir_reactive_manifest.h ....... Complete reactive manifest
ir/ir_json_v2.c ................. Implement v3 serialization
ir/parsers/tsx/ir_tsx_parser.c .. Preserve TSX event handlers
ir/parsers/c/ir_c_parser.c ...... Preserve C function bodies
ir/ir_kry_parser.c .............. Preserve .kry expressions
```

## Estimated Effort

- Phase 1 (Structures): 2 days
- Phase 2 (Logic System): 3 days
- Phase 3 (Reactive): 2 days
- Phase 4 (Serialization): 4 days
- Phase 5 (Parser Updates): 5 days

**Total: ~16 days (3 weeks)**
