# TKIR - Toolkit Intermediate Representation

TKIR is an intermediate representation between KIR and toolkit-based code generators.

## Architecture

```
KRY → KIR → TKIR → [tcltk | limbo | c]
```

**Web and markdown codegens** continue to use KIR directly (non-toolkit based).

## Purpose

TKIR reduces code duplication across toolkit-based targets by:

1. **Normalizing properties** - Sizes, colors, fonts represented as structured objects
2. **Resolving layout** - Pack/grid/place type and options pre-computed
3. **Centralizing handlers** - Event handlers in one place with multi-language implementations
4. **Propagating inheritance** - Background colors resolved and propagated
5. **Providing inspection** - JSON format easy to inspect/debug

## Implementation Status

**✅ Complete** - All phases implemented and deployed

- ✅ Phase 1: Foundation (TKIR builder)
- ✅ Phase 2: Tcl/Tk migration (84% code reduction: 1019 → 160 lines)
- ✅ Phase 3: Limbo migration (93% code reduction: 2352 → 162 lines)
- ✅ Phase 4: C codegen integration (TKIR emitter for standalone widget code)
- ✅ Phase 5: Legacy code removal (tcltk, limbo)

## Directory Structure

```
codegens/tkir/
├── tkir.h                # Core TKIR API
├── tkir.c                # Core implementation
├── tkir_builder.h        # KIR → TKIR transformation API
├── tkir_builder.c        # KIR → TKIR implementation
├── tkir_emitter.h        # Emitter interface
├── tkir_emitter.c        # Emitter registry
├── tkir_schema.json      # JSON schema
├── Makefile              # Standard make build
├── mkfile                # TaijiOS mkfile build
└── README.md             # This file
```

## TKIR Schema

```json
{
  "format": "tkir",
  "version": "1.0",
  "window": {
    "title": "Window Title",
    "width": 800,
    "height": 600,
    "resizable": true,
    "background": "#1a1a2e"
  },
  "widgets": [
    {
      "id": "widget-1",
      "tk_type": "frame",
      "kir_type": "Container",
      "properties": {
        "background": "#191970",
        "width": {"value": 200, "unit": "px"},
        "height": {"value": 100, "unit": "px"},
        "font": {"family": "Arial", "size": 12}
      },
      "layout": {
        "type": "pack",
        "parent": "root",
        "options": {
          "side": "top",
          "fill": "both",
          "expand": true
        }
      },
      "children": ["widget-2"],
      "events": ["handle_click"]
    }
  ],
  "handlers": [
    {
      "id": "handle_click",
      "event_type": "click",
      "widget_id": "widget-1",
      "implementations": {
        "tcl": "proc handle_click {} { ... }",
        "limbo": "handle_click(nil: ref Draw->Context, c: int, ...)"
      }
    }
  ]
}
```

## API Usage

### Direct TKIR Generation

```c
#include "tkir/tkir_builder.h"

// Build TKIR from KIR JSON
TKIRRoot* root = tkir_build_from_kir(kir_json, false);

// Convert to JSON string
char* tkir_json = tkir_root_to_json(root);

// Cleanup
tkir_root_free(root);
free(tkir_json);
```

### Tcl/Tk Codegen via TKIR

```c
#include "tcltk/tcltk_codegen.h"

// Method 1: Direct KIR → Tcl/Tk via TKIR (recommended)
char* output = tcltk_codegen_from_json_via_tkir(kir_json, &options);
free(output);

// Method 2: From TKIR JSON
char* output = tcltk_codegen_from_tkir(tkir_json, &options);
free(output);

// Method 3: File-based
bool success = tcltk_codegen_generate_via_tkir("app.kir", "app.tcl", &options);
```

### Limbo Codegen via TKIR

```c
#include "limbo/limbo_codegen.h"

// Method 1: Direct KIR → Limbo via TKIR (recommended)
char* output = limbo_codegen_from_json_via_tkir(kir_json, &options);
free(output);

// Method 2: From TKIR JSON
char* output = limbo_codegen_from_tkir(tkir_json, &options);
free(output);

// Method 3: File-based
bool success = limbo_codegen_generate_via_tkir("app.kir", "app.b", &options);
```

### Emitter Registration

```c
// Initialize emitters (during app startup)
tcltk_tkir_emitter_init();
limbo_tkir_emitter_init();

// Use emitters via registry
TKIREmitterContext* ctx = tkir_emitter_create(TKIR_EMITTER_TCLTK, NULL);
char* output = tkir_emit(ctx, root);
tkir_emitter_context_free(ctx);

// Cleanup (during app shutdown)
tcltk_tkir_emitter_cleanup();
limbo_tkir_emitter_cleanup();
```

## Benefits

- **44% code reduction** - Eliminates duplicated widget logic
- **Single source of truth** - Layout logic in one place
- **Easier debugging** - Inspectable TKIR JSON
- **Simpler targets** - Just write an emitter
- **Consistent behavior** - All targets use same TKIR

## Adding a New Target

1. Implement the `TKIREmitterVTable` interface
2. Create emitter context extending `TKIREmitterContext`
3. Register with `tkir_emitter_register()`

Example:

```c
static const TKIREmitterVTable my_emitter_vtable = {
    .emit_widget = my_emit_widget,
    .emit_handler = my_emit_handler,
    .emit_layout = my_emit_layout,
    .emit_property = my_emit_property,
    .emit_full = my_emit_full,
    .free_context = my_free_context,
};

void my_tkir_emitter_init(void) {
    tkir_emitter_register(TKIR_EMITTER_CUSTOM, &my_emitter_vtable);
}
```

## Building

```bash
cd codegens/tkir
make clean
make
```

Or with mk (TaijiOS):

```bash
cd codegens/tkir
mk clean
mk install
```

## Testing

```bash
# Generate TKIR from KIR
./tkir_test input.kir output.tkir

# Generate Tcl/Tk from TKIR
./tcltk_from_tkir output.tkir output.tcl

# Generate Limbo from TKIR
./limbo_from_tkir output.tkir output.b
```

## Code Reduction Achieved

| Codegen | Before | After | Reduction |
|---------|--------|-------|-----------|
| tcltk_codegen.c | 1,019 lines | 160 lines | 84% |
| limbo_codegen.c | 2,352 lines | 162 lines | 93% |
| **Total** | **3,371 lines** | **322 lines** | **90%** |

Note: C codegen retains legacy path for C-specific semantics (reactive state, structs, headers) that TKIR doesn't handle. TKIR provides widget generation option via `c_codegen_from_tkir()`.

## Implementation Notes

### Layout Types

- **Pack**: Default for Row/Column containers
- **Grid**: Explicit row/column positioning
- **Place**: Absolute positioning

### Property Normalization

All sizes represented as `{value, unit}`:
- `"200px"` → `{"value": 200, "unit": "px"}`
- `"50%"` → `{"value": 50, "unit": "%"}`

### Background Inheritance

Widgets without background inherit from parent:
- Root: Uses window background
- Children: Inherit from container

### Handler Deduplication

Same handler name generates only once:
- First occurrence: Generate full handler
- Subsequent: Reference by ID

## Future Work

1. **C Codegen Integration**: Optionally use TKIR for widget tree generation within full C programs
2. **Performance**: Optimize TKIR builder for large widget trees
3. **Validation**: Add JSON schema validation for TKIR
4. **Testing**: Comprehensive test coverage and regression tests
5. **Documentation**: More examples and usage patterns
