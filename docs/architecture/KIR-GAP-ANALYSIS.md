# KIR Format - Complete Gap Analysis

**Date:** 2025-01-27
**Status:** Pre-Implementation Analysis

## Executive Summary

The current KIR (Kryon Intermediate Representation) format has **critical gaps** that prevent round-trip conversion between languages. This document provides a comprehensive analysis of what exists, what's missing, and what needs to be fixed.

**Key Finding:** Binary (.kirb) and JSON (.kir) formats are **NOT equivalent** - they serialize different information, creating an asymmetric system.

---

## The Problem

```
Current State:
  .tsx → KIR (loses event handlers) → .nim ❌ BROKEN
  .c → KIR (loses function bodies) → .lua ❌ BROKEN
  .kry → KIR → .kry ✅ WORKS (only format that round-trips)
```

**Root Cause:** KIR JSON does not serialize `IRLogic` blocks with source code, only event references.

---

## Format Asymmetry Table

| Feature | Binary (.kirb) | JSON (.kir) | Impact |
|---------|---------------|-------------|---------|
| **Component tree** | ✅ | ✅ | Works |
| **Style properties** | ✅ | ✅ | Works |
| **Layout properties** | ✅ | ✅ | Works |
| **Animations/Transitions** | ✅ | ✅ | Works |
| **Event bindings** | ✅ | ✅ | Works |
| **IRLogic blocks** | ✅ | ❌ | **BROKEN** |
| **Reactive manifest** | ❌ | ✅ | **BROKEN** |
| **Component definitions** | ❌ | ✅ | **BROKEN** |
| **Source code preservation** | ❌ | ✅ | **BROKEN** |

**Result:** `.kirb` files preserve logic but lose reactive state. `.kir` files preserve reactive state but lose logic. Neither format is complete!

---

## What Exists in Code

### ✅ Structures Defined in `ir/ir_core.h`

**IRComponent (line 832-857):**
```c
typedef struct IRComponent {
    uint32_t id;
    IRComponentType type;
    IRStyle* style;           // ✅ Serialized
    IREvent* events;          // ✅ Serialized (event type + logic_id)
    IRLogic* logic;           // ❌ NOT serialized in JSON!
    IRLayout* layout;         // ✅ Serialized
    char* text_content;       // ✅ Serialized
    char* text_expression;    // ✅ Serialized (JSON only)
    // ... children, parent, etc.
};
```

**IREvent (line 636-642):**
```c
typedef struct IREvent {
    IREventType type;              // ✅ Serialized
    char* logic_id;                // ✅ Serialized (reference string)
    char* handler_data;            // ✅ Serialized
    uint32_t bytecode_function_id; // ✅ Serialized
    struct IREvent* next;
};
```

**IRLogic (line 645-650):**
```c
typedef struct IRLogic {
    char* id;                 // ✅ Binary only
    char* source_code;        // ✅ Binary only ❌ NOT in JSON!
    LogicSourceType type;     // ✅ Binary only
    struct IRLogic* next;
};
```

**IRReactiveManifest (line 1062-1095):**
```c
typedef struct {
    IRReactiveVarDescriptor* variables;      // ✅ JSON only
    IRReactiveBinding* bindings;             // ✅ JSON only
    IRReactiveConditional* conditionals;     // ✅ JSON only
    IRReactiveForLoop* for_loops;            // ✅ JSON only
    IRComponentDefinition* component_defs;   // ✅ JSON only
    IRSourceEntry* sources;                  // ✅ JSON only
};
```

### ✅ Advanced Logic System in `ir/ir_logic.h`

**IRLogicBlock (line 54-60):**
```c
typedef struct IRLogicBlock {
    IRLogicFunction** functions;       // ✅ Exists
    int function_count;
    IREventBinding** event_bindings;   // ✅ Exists
    int event_binding_count;
};
```

**IRLogicFunction (line 30-44):**
```c
typedef struct {
    char* name;

    // Universal representation
    bool has_universal;
    IRLogicParam* params;
    IRStatement** statements;

    // Embedded source code
    IRLogicSource sources[IR_LOGIC_MAX_SOURCES];  // ✅ Multi-language support!
    int source_count;
};
```

**IRLogicSource (line 19-22):**
```c
typedef struct {
    char* language;  // "nim", "lua", "js", "typescript", "c"
    char* source;    // THE ACTUAL SOURCE CODE
};
```

---

## What's Currently Serialized

### Binary Format (`ir_serialization.c`)

**Serializes:**
- ✅ Component tree (line 1065-1122)
- ✅ Style properties (line 720-837)
- ✅ Layout properties (line 367-435)
- ✅ Events (line 981-1001)
- ✅ **IRLogic blocks** (line 1024-1063) ⚠️ **CRITICAL**
- ✅ Animations (line 566-605)
- ✅ Transitions (line 607-648)

**Does NOT Serialize:**
- ❌ Reactive manifest
- ❌ Component definitions
- ❌ Source code metadata
- ❌ Text expressions

**Function:** `serialize_logic()` at line 1024:
```c
static bool serialize_logic(IRBuffer* buffer, IRLogic* logic) {
    write_string(buffer, logic->id);           // ✅
    write_string(buffer, logic->source_code);  // ✅ SOURCE CODE PRESERVED!
    write_uint8(buffer, (uint8_t)logic->type); // ✅
    return true;
}
```

### JSON Format (`ir/ir_json.c`)

**Serializes:**
- ✅ Component tree (line 723-1086)
- ✅ Style properties (line 723-1086)
- ✅ Layout properties (inline)
- ✅ Events with logic_id references (line 1038-1086)
- ✅ **Reactive manifest** (line 1232-1437)
- ✅ Component definitions (line 1146-1230)
- ✅ Source code entries (line 1500-1513)
- ✅ Text expressions (line 775-777)

**Does NOT Serialize:**
- ❌ **IRLogic blocks with source code**
- ❌ Logic source type
- ❌ Component->logic linked list

**Current Approach:**
```c
// Line 1038-1086: Serialize events
if (comp->events) {
    cJSON* events = cJSON_CreateArray();
    IREvent* evt = comp->events;
    while (evt) {
        cJSON* event_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(event_obj, "type", ir_event_type_to_string(evt->type));

        if (evt->logic_id) {
            cJSON_AddStringToObject(event_obj, "logic_id", evt->logic_id);  // ✅ Reference
        }
        // ❌ But the actual IRLogic with source_code is NOT serialized!
    }
}
```

---

## Critical Gaps - Detailed Analysis

### Gap 1: IRLogic Not Serialized in JSON ❌

**Current Code:**
- File: `ir/ir_json.c`
- Line 1448: `ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest)`
- **PROBLEM:** Function takes `manifest` but NOT `IRLogicBlock*` or `IRLogic*`

**What's Missing:**
```c
// Current signature (BROKEN)
char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest);

// Should be (FIXED)
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,  // ❌ MISSING!
    IRSourceMetadata* metadata  // ❌ MISSING!
);
```

**Impact:**
```c
// Example: Button with onClick handler
IRLogic* logic = malloc(sizeof(IRLogic));
logic->id = "handler_1";
logic->source_code = "count++";  // TypeScript source
logic->type = IR_LOGIC_NIM;

IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
button->logic = logic;  // Attach logic to component

// When serialized to JSON:
// - Event is serialized with logic_id = "handler_1" ✅
// - But the IRLogic with source_code "count++" is LOST ❌
```

### Gap 2: No Source Language Metadata ❌

**Current Code:**
- Events have `logic_id` references
- But no metadata about **which language** the logic is written in
- `IRLogic` has `LogicSourceType` (Nim/C/Lua/WASM) but it's not serialized in JSON

**What's Missing:**
```json
{
  "format": "kir",
  "metadata": {
    "source_language": "typescript"  // ❌ MISSING in current JSON!
  }
}
```

**File:** `ir/ir_serialization.h` line 19-21:
```c
// Version comment mentions v2.0, but no source language field
#define IR_FORMAT_VERSION_MAJOR 2
#define IR_FORMAT_VERSION_MINOR 0
```

### Gap 3: Logic Blocks vs Reactive Manifest Confusion ❌

**Two Systems Exist:**

1. **Old System:** `IRLogic` attached to components
   - Used in binary serialization
   - Simple id + source_code + type

2. **New System:** `IRReactiveManifest` + `IRLogicBlock`
   - Used in JSON serialization
   - More sophisticated with functions, params, multi-language support
   - But IRLogicBlock is **NOT** being serialized!

**Evidence:**
```c
// ir_logic.h defines IRLogicBlock (line 54-60)
typedef struct IRLogicBlock {
    IRLogicFunction** functions;      // Has multi-language sources!
    int function_count;
    IREventBinding** event_bindings;
    int event_binding_count;
};

// But ir_json.c DOES NOT serialize it!
// Only ir_krb.c uses it: KRBModule* krb_compile_from_ir(root, logic, manifest)
```

### Gap 4: No Event Handler Source Code in JSON ❌

**Example Workflow (BROKEN):**

```typescript
// Source: button.tsx
<Button onClick={() => console.log("clicked")}>Click</Button>
```

**Current KIR JSON Output:**
```json
{
  "components": [{
    "id": 1,
    "type": "Button",
    "text": "Click",
    "events": [{
      "type": "click",
      "logic_id": "handler_1"  // ✅ Reference exists
    }]
  }]
  // ❌ But where is the source code "() => console.log('clicked')"?
}
```

**Binary Output (Works!):**
```
Component ID: 1
Type: Button
Logic:
  ID: handler_1
  Source: () => console.log("clicked")  // ✅ PRESERVED!
  Type: WASM/JS
```

### Gap 5: Reactive Manifest Not in Binary ❌

**Problem:** Binary format loses reactive state

**Example:**
```kry
Container {
  @state count: int = 0

  Button {
    onClick: { count += 1 }
  }
}
```

**Binary (.kirb):**
```
Component: Container
  Logic: onClick → { count += 1 }  ✅
  Reactive State: ???  ❌ MISSING
```

**JSON (.kir):**
```json
{
  "reactive_manifest": {
    "states": [{
      "name": "count",
      "type": "int",
      "initial_value": "0"  // ✅ PRESERVED
    }]
  },
  "logic_blocks": []  // ❌ EMPTY! onClick logic is LOST
}
```

---

## Complete List of Missing Features

### In JSON Format (`ir/ir_json.c`)

1. ❌ **IRLogic serialization** (line ~1450 - should be added)
   - Need to serialize component->logic linked list
   - Include id, source_code, type for each logic block

2. ❌ **IRLogicBlock serialization** (not present)
   - Should serialize the complete IRLogicBlock with functions
   - Include multi-language source code from IRLogicSource[]

3. ❌ **Source language metadata** (line 1450 - metadata section)
   - Add "source_language" field to metadata
   - Track whether original was tsx/c/nim/lua/kry

4. ❌ **Logic source type** (not tracked)
   - When events reference logic_id, no type info
   - Can't tell if it's TypeScript, C, Nim, etc.

5. ❌ **Event handler → Logic connection** (incomplete)
   - Events have logic_id reference ✅
   - But logic blocks are not serialized ❌
   - Broken link!

### In Binary Format (`ir_serialization.c`)

1. ❌ **IRReactiveManifest** (not serialized)
   - Need to add reactive variables (line ~1250)
   - Need to add bindings (line ~1250)
   - Need to add conditionals (line ~1250)

2. ❌ **Component definitions** (not serialized)
   - Custom component templates lost
   - Props definitions lost
   - State var definitions lost

3. ❌ **Source code entries** (not serialized)
   - Round-trip preservation lost
   - Multi-language source tracking lost

4. ❌ **Text expressions** (not serialized)
   - `text_expression` field (e.g., "{{count}}") not in binary

### In Both Formats

1. ❌ **Unified logic representation**
   - Binary uses IRLogic
   - JSON references logic_id but doesn't serialize IRLogic
   - Need to unify!

2. ❌ **Metadata structure** (incomplete)
   - No source_file tracking
   - No compiler_version
   - No timestamp

---

## Required Changes

### 1. Update Serialization Signature

**File:** `ir/ir_serialization.h` (line 40)

**Change:**
```c
// OLD (current)
char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest);

// NEW (fixed)
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,        // ❌ ADD THIS
    IRSourceMetadata* source_metadata // ❌ ADD THIS
);
```

### 2. Create IRSourceMetadata Structure

**File:** `ir/ir_core.h` (add after line 882)

```c
// Source file metadata for round-trip
typedef struct {
    char* source_language;    // "tsx", "c", "nim", "lua", "kry"
    char* source_file;        // Path to original file
    char* compiler_version;   // "kryon-1.0.0"
    char* timestamp;          // ISO8601 timestamp
} IRSourceMetadata;
```

### 3. Serialize IRLogic in JSON

**File:** `ir/ir_json.c` (add after line 1086)

```c
static cJSON* json_serialize_logic_blocks(IRLogicBlock* logic) {
    if (!logic) return NULL;

    cJSON* blocks = cJSON_CreateArray();

    // Serialize functions with multi-language sources
    for (int i = 0; i < logic->function_count; i++) {
        IRLogicFunction* func = logic->functions[i];

        cJSON* func_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(func_obj, "name", func->name);

        // Serialize embedded sources
        cJSON* sources = cJSON_CreateArray();
        for (int j = 0; j < func->source_count; j++) {
            cJSON* src = cJSON_CreateObject();
            cJSON_AddStringToObject(src, "language", func->sources[j].language);
            cJSON_AddStringToObject(src, "source", func->sources[j].source);
            cJSON_AddItemToArray(sources, src);
        }
        cJSON_AddItemToObject(func_obj, "sources", sources);

        cJSON_AddItemToArray(blocks, func_obj);
    }

    return blocks;
}
```

### 4. Update Main Serialization Function

**File:** `ir/ir_json.c` (line 1448)

```c
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,
    IRSourceMetadata* metadata
) {
    cJSON* json = cJSON_CreateObject();

    // Format identifier
    cJSON_AddStringToObject(json, "format", "kir");

    // METADATA SECTION (NEW)
    if (metadata) {
        cJSON* meta = cJSON_CreateObject();
        cJSON_AddStringToObject(meta, "source_language", metadata->source_language);
        cJSON_AddStringToObject(meta, "source_file", metadata->source_file);
        cJSON_AddStringToObject(meta, "compiler_version", metadata->compiler_version);
        if (metadata->timestamp) {
            cJSON_AddStringToObject(meta, "timestamp", metadata->timestamp);
        }
        cJSON_AddItemToObject(json, "metadata", meta);
    }

    // REACTIVE MANIFEST (existing)
    if (manifest) {
        cJSON* reactive = json_serialize_reactive_manifest(manifest);
        cJSON_AddItemToObject(json, "reactive_manifest", reactive);
    }

    // LOGIC BLOCKS (NEW!)
    if (logic_block) {
        cJSON* logic = json_serialize_logic_blocks(logic_block);
        cJSON_AddItemToObject(json, "logic_blocks", logic);
    }

    // COMPONENTS (existing)
    cJSON* components = json_serialize_component_tree(root);
    cJSON_AddItemToObject(json, "components", components);

    char* result = cJSON_Print(json);
    cJSON_Delete(json);
    return result;
}
```

### 5. Add Reactive Manifest to Binary

**File:** `ir_serialization.c` (add after line 1100)

```c
static bool serialize_reactive_manifest(IRBuffer* buffer, IRReactiveManifest* manifest) {
    if (!manifest) {
        write_uint32(buffer, 0);  // No manifest
        return true;
    }

    // Variable count
    write_uint32(buffer, manifest->variable_count);

    // Serialize each variable
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        write_uint32(buffer, var->id);
        write_string(buffer, var->name);
        write_uint8(buffer, (uint8_t)var->type);

        // Serialize metadata
        if (var->type_string) {
            write_string(buffer, var->type_string);
        }
        if (var->initial_value_json) {
            write_string(buffer, var->initial_value_json);
        }
    }

    // Bindings count
    write_uint32(buffer, manifest->binding_count);

    // ... serialize bindings, conditionals, for-loops

    return true;
}
```

---

## Expected KIR JSON Format (Complete)

```json
{
  "format": "kir",

  "metadata": {
    "source_language": "typescript",
    "source_file": "button.tsx",
    "compiler_version": "kryon-1.0.0",
    "timestamp": "2025-01-27T12:00:00Z"
  },

  "reactive_manifest": {
    "states": [{
      "id": "state_1",
      "name": "count",
      "type": "int",
      "initial_value": "0",
      "type_string": "number",
      "scope": "local"
    }]
  },

  "logic_blocks": [{
    "id": "logic_1",
    "name": "handleClick",
    "sources": [{
      "language": "typescript",
      "source": "() => setCount(count + 1)"
    }, {
      "language": "nim",
      "source": "proc handleClick() = count.set(count.get() + 1)"
    }]
  }],

  "components": [{
    "id": 1,
    "type": "Button",
    "text": "Click",
    "events": [{
      "type": "click",
      "logic_id": "logic_1"
    }]
  }]
}
```

---

## Implementation Priority

### Phase 1: Critical Fixes (Week 1)
1. Add `IRSourceMetadata` structure
2. Update `ir_serialize_json()` signature to `ir_serialize_json_complete()`
3. Add metadata section to JSON output
4. Serialize IRLogic blocks in JSON (basic version)

### Phase 2: Logic System (Week 2)
1. Serialize IRLogicBlock with multi-language sources
2. Connect events → logic with full source preservation
3. Update parsers to populate IRLogicBlock
4. Test round-trip: tsx → KIR → nim

### Phase 3: Binary Parity (Week 3)
1. Add reactive manifest to binary format
2. Add component definitions to binary
3. Unify binary and JSON formats
4. Ensure .kirb and .kir are equivalent

---

## Success Criteria

✅ **Round-Trip Works:**
```bash
# TypeScript → KIR → Nim
kryon compile button.tsx -o button.kir
cat button.kir | jq '.logic_blocks[0].sources[0].source'
# Output: "() => setCount(count + 1)"

kryon codegen nim button.kir -o button.nim
# Output contains: proc handleClick() = ...

kryon compile button.nim -o button2.kir
diff button.kir button2.kir  # Semantically equivalent
```

✅ **Event Handlers Preserved:**
```bash
cat app.kir | jq '.logic_blocks[].sources[]'
# Shows source code for ALL languages
```

✅ **Metadata Present:**
```bash
cat app.kir | jq '.metadata.source_language'
# Output: "typescript"
```

✅ **Format Equivalence:**
```bash
kryon compile app.kry -o app.kir
kryon compile app.kry -o app.kirb
# Both preserve same information (just different encoding)
```

---

## Files to Modify

```
ir/ir_core.h ..................... Add IRSourceMetadata (line ~883)
ir/ir_serialization.h ............ Update function signatures (line 40-41)
ir/ir_serialization.c ............ Add reactive manifest to binary (line ~1250)
ir/ir_json.c ..................... Add logic block serialization (line ~1450)
ir/ir_logic.h .................... (Already complete)
ir/parsers/tsx/*.c ............... Update to populate IRLogicBlock
ir/parsers/c/*.c ................. Update to populate IRLogicBlock
```

---

## Estimated Effort

- Phase 1 (Critical Fixes): 5 days
- Phase 2 (Logic System): 7 days
- Phase 3 (Binary Parity): 4 days

**Total: ~16 days (3 weeks)**

---

## Next Steps

1. Create `IRSourceMetadata` structure
2. Update serialization signatures
3. Implement `json_serialize_logic_blocks()`
4. Test with simple button example
5. Verify round-trip works
6. Update all parsers to populate logic blocks
7. Update all codegens to consume logic blocks

This analysis provides the complete roadmap for fixing KIR!
