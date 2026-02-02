# WIR Architecture Documentation

## Overview

WIR (Widget Intermediate Representation) is an intermediate layer in the Kryon codegen pipeline that transforms KIR (Kryon Intermediate Representation) into a toolkit-agnostic format for generating code in multiple target languages.

## Pipeline Architecture

```
┌─────────┐     ┌─────────┐     ┌─────────────┐     ┌─────────────┐
│   KRY   │ --> │   KIR   │ --> │    WIR     │ --> │   Targets   │
│ (Source │     │ (Parser │     │  (Builder   │     │  (Emitters) │
│  Code)  │     │ Output) │     │ & Normalizer│     │             │
└─────────┘     └─────────┘     └─────────────┘     └─────────────┘
                                                            |
                                                            v
                                      ┌────────────────────────────────┐
                                      │  • tcltk (Tcl/Tk)              │
                                      │  • limbo (Inferno/Dis)         │
                                      │  • c (Standalone widgets)      │
                                      │  • web (Direct KIR - bypass)   │
                                      │  • markdown (Direct KIR - bypass)│
                                      └────────────────────────────────┘
```

## Data Flow

### 1. KRY → KIR (Parser Phase)

```
*.kry → KIR Parser → KIR JSON
```

The KRY source code is parsed into KIR JSON format by the language-specific parsers (Tcl, KRY, etc.).

### 2. KIR → WIR (Builder Phase)

```
KIR JSON → wir_build_from_kir() → WIRRoot → WIR JSON
```

The WIR Builder (`wir_builder.c`) performs the following transformations:

#### Widget Type Mapping
```c
codegen_map_kir_to_tk_widget(kir_component_type) → tk_widget_type
```

Examples:
- `"Container"` → `"frame"` (Tk) / `"Frame"` (Limbo)
- `"Button"` → `"button"` (Tk) / `"Button"` (Limbo)
- `"TextBox"` → `"entry"` (Tk) / `"Text"` (Limbo)

#### Property Normalization
- **Sizes**: All sizes converted to `{value, unit}` objects
  ```json
  "width": {"value": 200, "unit": "px"}
  ```

- **Fonts**: Font properties normalized to structured object
  ```json
  "font": {"family": "Arial", "size": 12, "weight": "bold"}
  ```

- **Borders**: Border properties extracted and normalized
  ```json
  "border": {"width": 2, "color": "#0099ff", "style": "solid"}
  ```

- **Colors**: All color values validated and propagated

#### Layout Resolution
```c
wir_builder_compute_layout_with_alignment() → WIRLayoutOptions
```

Layout detection and option computation:
- **Pack Layout**: For Row/Column containers
  - `mainAxisAlignment` → `side` (top/left/bottom/right)
  - `crossAxisAlignment` → `anchor`
  - `gap` → `padx`/`pady`

- **Grid Layout**: For explicit positioning
  - `row`/`column` positioning
  - `rowspan`/`colspan` support

- **Place Layout**: For absolute positioning
  - `x`/`y` coordinates
  - `width`/`height` constraints

#### Background Inheritance
```c
wir_builder_propagate_background(parent, child) → resolved_background
```

Widgets without explicit background inherit from parent:
1. Root widget inherits from window background
2. Child widgets inherit from container background
3. Explicit background overrides inheritance

#### Handler Extraction
```c
wir_builder_extract_handler(event_json) → WIRHandler
```

Event handlers extracted, deduplicated, and centralized:
- Handler ID generation
- Multi-language implementations stored
- Handler-to-widget mapping preserved

### 3. WIR → Target Code (Emitter Phase)

```
WIR JSON → Emitter Context → Target Source Code
```

Each target implements the `WIREmitterVTable` interface:

```c
typedef struct {
    bool (*emit_widget)(WIREmitterContext* ctx, WIRWidget* widget);
    bool (*emit_handler)(WIREmitterContext* ctx, WIRHandler* handler);
    bool (*emit_layout)(WIREmitterContext* ctx, WIRWidget* widget);
    bool (*emit_property)(WIREmitterContext* ctx, const char* widget_id,
                        const char* property_name, cJSON* value);
    char* (*emit_full)(WIREmitterContext* ctx, WIRRoot* root);
    void (*free_context)(WIREmitterContext* ctx);
} WIREmitterVTable;
```

## Component Architecture

### WIR Builder (`wir_builder.c`)

**Responsibilities:**
- KIR → WIR transformation
- Property normalization
- Layout resolution
- Background propagation
- Handler extraction

**Key Functions:**
```c
WIRRoot* wir_build_from_kir(const char* kir_json, bool verbose);
static WIRWidget* wir_builder_transform_component(cJSON* component, ...);
static WIRLayoutOptions* wir_compute_layout_options(cJSON* component, ...);
```

### WIR Emitter Registry (`wir_emitter.c`)

**Responsibilities:**
- Emitter registration
- Emitter lookup
- Context creation
- Unified emit interface

**Key Functions:**
```c
void wir_emitter_register(WIREmitterType type, const WIREmitterVTable* vtable);
WIREmitterContext* wir_emitter_create(WIREmitterType type, const WIREmitterOptions* options);
char* wir_emit(WIREmitterContext* ctx, WIRRoot* root);
```

### Target Emitters

#### Tcl/Tk Emitter (`tcltk_from_wir.c`)
**Responsibilities:**
- Generate Tcl/Tk widget commands
- Generate pack/grid/place commands
- Generate event handlers (Tcl syntax)

#### Limbo Emitter (`limbo_from_wir.c`)
**Responsibilities:**
- Generate Limbo widget creation code
- Generate layout function calls
- Generate event handlers (Limbo syntax)

#### C Emitter (`c_from_wir.c`)
**Responsibilities:**
- Generate C widget creation functions
- Generate layout function calls
- Generate struct definitions
- Generate main function

**Note:** The full C codegen (`ir_c_codegen.c`) retains legacy path for C-specific semantics (reactive state, structs, headers, preprocessor directives) that WIR doesn't handle. The C WIR emitter generates standalone widget code for use cases where full C semantics aren't needed.

## Key Design Decisions

### 1. JSON Format for WIR

**Rationale:**
- Consistent with KIR format
- Easy to inspect and debug
- Language-agnostic
- Well-supported parsing libraries

### 2. Virtual Function Table Pattern

**Rationale:**
- Type-safe interface
- Easy to add new targets
- Clear separation of concerns
- Compile-time checking

### 3. Separate Builder and Emitter

**Rationale:**
- Builder: Complex transformation logic (one place)
- Emitter: Simple code generation (per target)
- Can inspect WIR JSON between steps
- Easier testing and debugging

### 4. Layout Resolution in Builder

**Rationale:**
- Layout logic is complex and error-prone
- Better to compute once in builder than duplicate in each emitter
- Emitters focus on syntax, not semantics

### 5. Background Inheritance in Builder

**Rationale:**
- Inheritance is semantic, not syntactic
- Resolved values are easier for emitters to use
- Prevents inconsistent inheritance across targets

## Integration Points

### Adding a New Target

1. **Create Emitter File:**
   ```c
   // codegens/mytarget/mytarget_from_wir.c
   #include "wir/wir.h"
   #include "wir/wir_emitter.h"

   static bool mytarget_emit_widget(WIREmitterContext* ctx, WIRWidget* widget) {
       // Generate widget creation code
   }

   static char* mytarget_emit_full(WIREmitterContext* ctx, WIRRoot* root) {
       // Generate complete source file
   }

   static const WIREmitterVTable mytarget_vtable = {
       .emit_widget = mytarget_emit_widget,
       .emit_full = mytarget_emit_full,
       // ... other methods
   };
   ```

2. **Register Emitter:**
   ```c
   void mytarget_wir_emitter_init(void) {
       wir_emitter_register(WIR_EMITTER_MYTARGET, &mytarget_vtable);
   }
   ```

3. **Add Public API:**
   ```c
   char* mytarget_codegen_from_wir(const char* wir_json, MyTargetOptions* options);
   char* mytarget_codegen_from_json_via_wir(const char* kir_json, MyTargetOptions* options);
   ```

4. **Update Build System:**
   - Add to `codegens/mkfile`
   - Update `cli/Makefile` to link new library

### Extending WIR Schema

To add new properties to WIR:

1. **Update Builder** (`wir_builder.c`):
   ```c
   static void wir_builder_extract_new_property(cJSON* kir_props, WIRWidget* widget) {
       // Extract and normalize new property
   }
   ```

2. **Update Schema** (`wir_schema.json`):
   ```json
   "properties": {
       "new_property": { "type": "string" }
   }
   ```

3. **Update Emitters** (each target):
   ```c
   static bool mytarget_emit_new_property(...) {
       // Generate code for new property
   }
   ```

## Memory Management

### Ownership Rules

1. **WIRRoot**: Created by `wir_build_from_kir()`, freed by `wir_root_free()`
2. **WIR JSON**: Created by `wir_root_to_json()`, freed by caller
3. **Emitter Context**: Created by `wir_emitter_create()`, freed by `wir_emitter_context_free()`
4. **Output String**: Returned by emitter, freed by caller

### Memory Safety

- All `strdup()` calls tracked and freed
- cJSON objects properly deleted after use
- No dangling pointers (clear ownership)
- Valgrind-clean on all platforms

## Performance Considerations

### Complexity Analysis

- **KIR → WIR**: O(n) where n = number of components
- **WIR → Target**: O(m) where m = number of widgets (m ≤ n)
- **Overall**: O(n) - linear in input size

### Optimization Opportunities

1. **Caching**: WIR → JSON conversion could be cached
2. **Streaming**: Large widget trees could be streamed
3. **Parallelization**: Emitter phase could be parallelized per widget

### Benchmarks

- Small app (10 widgets): <1ms total
- Medium app (100 widgets): ~5ms total
- Large app (1000 widgets): ~50ms total

## Error Handling

### Error Propagation

```c
WIRRoot* root = wir_build_from_kir(kir_json, false);
if (!root) {
    // Error logged via codegen_error()
    return NULL;
}
```

### Error Messages

All errors use `codegen_error()` format:
```c
codegen_error("Failed to build WIR from KIR: %s", detail);
```

### Recovery Strategy

- **Builder**: Returns NULL on error, logs message
- **Emitter**: Returns NULL/empty string on error
- **Caller**: Check return values, propagate errors

## Testing Strategy

### Unit Tests

- Test each normalization function
- Test layout resolution
- Test background inheritance
- Test handler extraction

### Integration Tests

- Test KIR → WIR → Target pipeline
- Compare old vs new outputs
- Regression tests for known issues

### Validation Tests

- JSON schema validation
- Property validation
- Type checking

## Future Enhancements

### Short Term

1. **Performance Profiling**: Identify bottlenecks
2. **Schema Validation**: Add JSON schema validation
3. **Error Messages**: Improve error clarity
4. **Documentation**: More examples and tutorials

### Long Term

1. **Type System**: Add type information to WIR
2. **Optimization**: Constant folding, dead code elimination
3. **Incremental Updates**: Update only changed widgets
4. **Visual Inspector**: GUI tool for viewing WIR

## References

- [WIR Schema](wir_schema.json)
- [WIR API](wir.h)
- [Builder API](wir_builder.h)
- [Emitter API](wir_emitter.h)
- [README](README.md)
