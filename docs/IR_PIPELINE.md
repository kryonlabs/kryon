# Kryon IR Pipeline Guide

**Version:** 2.0
**Last Updated:** 2025-12-01

## Table of Contents

1. [Overview](#overview)
2. [Pipeline Architecture](#pipeline-architecture)
3. [Modular Design](#modular-design)
4. [File Formats](#file-formats)
5. [Frontend Guidelines](#frontend-guidelines)
6. [Backend Guidelines](#backend-guidelines)
7. [Testing Guidelines](#testing-guidelines)

## Overview

The Kryon IR (Intermediate Representation) pipeline provides a clean separation between frontends (Nim, Lua, C) and backends (SDL3, Terminal, Web). This architecture allows:

- **Any frontend can target any backend** - Write UI in your preferred language
- **Efficient binary format** - Fast loading and small file sizes
- **Human-readable JSON format** - Debugging and inspection
- **Modular and testable** - Each pipeline stage is independently verifiable

## Pipeline Architecture

```
┌──────────────────────────────────────────────────────┐
│ FRONTENDS (Language Bindings)                        │
├──────────────────────────────────────────────────────┤
│  Nim DSL → ir_builder API → .kir (JSON v2)          │
│  Lua (future) → ir_builder API → .kir (JSON v2)     │
│  C (future) → ir_builder API → .kir (JSON v2)       │
└──────────────────────────────────────────────────────┘
                      ↓
┌──────────────────────────────────────────────────────┐
│ IR COMPILATION LAYER (Required Step)                 │
├──────────────────────────────────────────────────────┤
│  .kir (JSON) ← Standard format from frontends        │
│       ↓                                              │
│  Format detection (ir_format_detect.c)              │
│       ↓                                              │
│  Validation (ir_validation.c)                       │
│       ↓                                              │
│  ir_read_json_v2_file() → IRComponent*              │
│       ↓                                              │
│  ir_write_binary_file() → .kirb                     │
│       ↓                                              │
│  .kirb (Binary v2.0) ← ONLY format backends read    │
└──────────────────────────────────────────────────────┘
                      ↓
┌──────────────────────────────────────────────────────┐
│ BACKENDS (Renderers) - ONLY READ .kirb               │
├──────────────────────────────────────────────────────┤
│  SDL3 → Loads .kirb ONLY → Renders desktop         │
│  Terminal → Loads .kirb ONLY → Renders terminal    │
│  Web → Loads .kirb ONLY → Generates HTML/CSS       │
│                                                      │
│  If given .kir: auto-compile to .kirb first         │
└──────────────────────────────────────────────────────┘
```

### Key Principles

1. **Frontends ALWAYS output .kir (JSON)** - Human-readable, standardized format
2. **Backends ONLY read .kirb (binary)** - Efficient, optimized format
3. **IR layer handles conversion** - Automatic .kir → .kirb compilation
4. **.kir format is IDENTICAL across all frontends** - No frontend-specific extensions

## Modular Design

The IR pipeline is designed as **5 independent modules**, each with clear interfaces and independent testability:

### Module 1: Frontend Module (Source → .kir)

**Purpose:** Convert frontend DSL/code to standardized .kir JSON format

**Input:** Source code (Nim DSL, Lua, C)
**Output:** `.kir` file (JSON v2 format)
**Interface:** `ir_builder` API

**Core Functions:**
- `ir_create_component()` - Create IR components
- `ir_add_child()` - Build component tree
- `ir_set_style()` - Apply styling
- `ir_write_json_v2_file_with_manifest()` - Serialize to .kir

**Testing:**
- Verify DSL macros expand correctly
- Verify component tree structure
- Verify .kir output is valid JSON
- Verify all properties are serialized

### Module 2: Serialization Module (.kir → in-memory IR)

**Purpose:** Deserialize JSON IR into memory structures

**Input:** `.kir` file (JSON)
**Output:** `IRComponent*` tree (in-memory)
**Interface:** `ir_read_json_v2_file()`

**Implementation:** `ir/ir_json_v2.c`

**Core Functions:**
- `ir_read_json_v2_file()` - Load JSON IR
- `ir_read_json_v2_file_with_manifest()` - Load JSON IR with reactive manifest

**Testing:**
- Roundtrip tests: .kir → IR → .kir (verify equality)
- Malformed JSON handling
- Large file handling
- Unicode/special character handling

### Module 3: Validation Module (IR validation)

**Purpose:** Verify IR correctness and integrity

**Input:** `IRComponent*` tree
**Output:** Validation result (bool + error list)
**Interface:** `ir_validate_component()`

**Implementation:** `ir/ir_validation.c`

**Validation Checks:**
- Format validation (magic numbers, version)
- Structure validation (no orphans, no cycles)
- Semantic validation (property constraints, layout validity)
- Reactive manifest consistency

**Testing:**
- Valid IR trees (should pass)
- Invalid IR trees (should fail with specific errors)
- Edge cases (empty trees, deep nesting, etc.)

### Module 4: Binary Compilation Module (in-memory IR → .kirb)

**Purpose:** Serialize IR to efficient binary format

**Input:** `IRComponent*` tree
**Output:** `.kirb` file (Binary v2.0)
**Interface:** `ir_write_binary_file()`

**Implementation:** `ir/ir_serialization.c`

**Core Functions:**
- `ir_write_binary_file()` - Write binary IR
- `ir_write_binary_file_with_manifest()` - Write binary IR with manifest
- `ir_read_binary_file()` - Load binary IR
- `ir_read_binary_file_with_manifest()` - Load binary IR with manifest

**Binary Format Advantages:**
- 5-10× smaller file size vs JSON
- Faster parsing (no text processing)
- Direct memory layout
- Checksum verification

**Testing:**
- Roundtrip tests: IR → .kirb → IR (verify equality)
- File corruption detection
- Version compatibility
- Size/performance benchmarks

### Module 5: Backend Loading Module (.kirb → rendering)

**Purpose:** Load binary IR and render UI

**Input:** `.kirb` file
**Output:** Rendered UI (SDL3/Terminal/Web)
**Interface:** Backend-specific (e.g., `desktop_ir_renderer_render_frame()`)

**Implementation:**
- `backends/desktop/ir_desktop_renderer.c` - SDL3 backend
- `renderers/terminal/terminal_backend.c` - Terminal backend
- `backends/web/ir_web_renderer.c` - Web backend (incomplete)

**Core Functions:**
- `ir_read_binary_file()` - Load .kirb file
- Backend render function - Render loaded IR

**Testing:**
- Load various .kirb files
- Verify rendering output
- Performance benchmarks
- Error handling (corrupt files, missing resources)

## File Formats

### .kir Format (JSON v2)

**Purpose:** Human-readable IR for debugging and frontend output

**File Extension:** `.kir`
**MIME Type:** `application/json`
**Encoding:** UTF-8

**Structure:**
```json
{
  "id": 1,
  "type": "CONTAINER",
  "style": {
    "width": "100%",
    "height": "auto",
    "background": "#ffffff",
    "padding": 16
  },
  "children": [...]
}
```

**Specification:** See `KIR_FORMAT_V2.md` for complete JSON schema

### .kirb Format (Binary v2.0)

**Purpose:** Efficient binary IR for backend consumption

**File Extension:** `.kirb`
**Magic Number:** `0x4B495242` ("KIRB")
**Endianness:** Little-endian

**Structure:**
```
Header (12 bytes):
  - Magic: 0x4B495242 ("KIRB")
  - Version: 2.0
  - Flags: (manifest present, etc.)
  - Endianness check

Component Tree:
  - Recursive component serialization
  - Efficient property encoding
  - Child reference indices

Reactive Manifest (optional):
  - Variable descriptors
  - Component bindings
```

**Specification:** See `KIR_FORMAT_V2.md` for complete binary format spec

## Frontend Guidelines

### Outputting .kir Files

All frontends MUST output standardized .kir (JSON v2) format:

**Nim Example:**
```nim
# In your frontend runtime (after building component tree)
let serializeTarget = getEnv("KRYON_SERIALIZE_IR")
if serializeTarget != "":
  if ir_write_json_v2_file_with_manifest(root, manifest, cstring(serializeTarget)):
    echo "✓ IR serialized successfully"
  else:
    echo "✗ Failed to serialize IR"
    quit(1)
```

**C Example:**
```c
// After building component tree
IRComponent* root = build_ui();
bool success = ir_write_json_v2_file_with_manifest(root, NULL, "app.kir");
if (!success) {
    fprintf(stderr, "Failed to serialize IR\n");
    return 1;
}
```

### Standardization Rules

1. **Use ir_builder API exclusively** - No direct struct manipulation
2. **Output .kir format only** - Never output .kirb from frontends
3. **Include reactive manifest** - Use `ir_write_json_v2_file_with_manifest()` if your frontend supports reactive state
4. **UTF-8 encoding** - All text must be UTF-8
5. **No frontend-specific extensions** - .kir format must be identical across all frontends

## Backend Guidelines

### Loading .kirb Files

Backends MUST ONLY read .kirb (binary) format:

**Desktop Backend Example:**
```c
// Load binary IR
IRComponent* root = ir_read_binary_file("app.kirb");
if (!root) {
    fprintf(stderr, "Failed to load IR\n");
    return false;
}

// Render
desktop_ir_renderer_render_frame(renderer, root);
```

### Auto-Compilation Workflow

If a backend receives .kir instead of .kirb, it should auto-compile:

```c
IRFormat format = ir_detect_format(filename);

if (format == IR_FORMAT_JSON) {
    // Auto-compile .kir → .kirb
    IRComponent* root = ir_read_json_v2_file(filename);

    // Generate .kirb filename
    char kirb_filename[256];
    snprintf(kirb_filename, sizeof(kirb_filename), "%s.kirb", filename);

    // Write binary
    ir_write_binary_file(root, kirb_filename);

    // Load binary for rendering
    // (backend continues with .kirb)
}
else if (format == IR_FORMAT_BINARY) {
    // Direct .kirb load
    IRComponent* root = ir_read_binary_file(filename);
}
```

### Backend Requirements

1. **Only load .kirb** - Binary format is mandatory for backends
2. **Auto-compile if needed** - Convert .kir → .kirb if given JSON
3. **Use format detection** - Call `ir_detect_format()` to identify file type
4. **Validate before rendering** - Call `ir_validate_component()` on loaded IR
5. **Clean error messages** - Report clear errors for invalid IR

## Testing Guidelines

### Unit Testing Each Module

**Frontend Module:**
```nim
# tests/frontend/test_nim_dsl.nim
test "Button DSL generates correct IR":
  let component = Button:
    title = "Click me"
    onClick = proc() = echo "Clicked"

  check component.componentType == IR_COMPONENT_BUTTON
  check component.textContent == "Click me"
```

**Serialization Module:**
```c
// tests/serialization/test_json_serialization.c
void test_json_roundtrip() {
    IRComponent* original = create_test_component();

    // Serialize to JSON
    ir_write_json_v2_file(original, "test.kir");

    // Deserialize
    IRComponent* loaded = ir_read_json_v2_file("test.kir");

    // Verify equality
    assert_components_equal(original, loaded);
}
```

**Validation Module:**
```c
// tests/validation/test_ir_validation.c
void test_detect_circular_reference() {
    IRComponent* parent = ir_create_component(IR_COMPONENT_CONTAINER);
    IRComponent* child = ir_create_component(IR_COMPONENT_CONTAINER);

    ir_add_child(parent, child);
    ir_add_child(child, parent);  // Create cycle

    assert(!ir_validate_component(parent));  // Should fail
}
```

**Binary Compilation Module:**
```c
// tests/conversion/test_kir_to_kirb.c
void test_kir_to_kirb_conversion() {
    // Load .kir
    IRComponent* root = ir_read_json_v2_file("app.kir");

    // Write .kirb
    ir_write_binary_file(root, "app.kirb");

    // Verify file size reduction
    assert(get_file_size("app.kirb") < get_file_size("app.kir"));

    // Roundtrip
    IRComponent* loaded = ir_read_binary_file("app.kirb");
    assert_components_equal(root, loaded);
}
```

**Backend Module:**
```c
// tests/backend/test_sdl3_loading.c
void test_backend_loads_kirb() {
    IRComponent* root = ir_read_binary_file("app.kirb");
    assert(root != NULL);

    // Render and verify
    bool success = desktop_ir_renderer_render_frame(renderer, root);
    assert(success);
}
```

### Integration Testing

**Full Pipeline Test:**
```c
// tests/integration/test_full_pipeline.c
void test_end_to_end_pipeline() {
    // 1. Frontend: Nim → .kir
    system("nim c -r app.nim");  // Outputs app.kir

    // 2. Conversion: .kir → .kirb
    IRComponent* root = ir_read_json_v2_file("app.kir");
    ir_write_binary_file(root, "app.kirb");

    // 3. Backend: Load .kirb and render
    IRComponent* loaded = ir_read_binary_file("app.kirb");
    assert(desktop_ir_renderer_render_frame(renderer, loaded));
}
```

### Makefile Test Targets

```makefile
# tests/Makefile
.PHONY: test-serialization test-validation test-conversion test-backend test-integration test-all

test-serialization:
	@echo "Running serialization module tests..."
	./tests/serialization/test_json_serialization
	./tests/serialization/test_binary_serialization

test-validation:
	@echo "Running validation module tests..."
	./tests/validation/test_ir_validation

test-conversion:
	@echo "Running conversion module tests..."
	./tests/conversion/test_kir_to_kirb

test-backend:
	@echo "Running backend module tests..."
	./tests/backend/test_sdl3_loading
	./tests/backend/test_terminal_loading

test-integration:
	@echo "Running integration tests..."
	./tests/integration/test_full_pipeline

test-all: test-serialization test-validation test-conversion test-backend test-integration
	@echo "✅ All tests passed!"
```

## CLI Usage

### Compile Frontend to .kir

```bash
# Nim frontend (outputs .kir by default)
kryon compile app.nim
# → app.kir

# With explicit format
kryon compile app.nim --format=json
# → app.kir
```

### Convert .kir to .kirb

```bash
# Manual conversion
kryon convert app.kir app.kirb
# → Converts JSON to binary

# Check file sizes
ls -lh app.kir app.kirb
# app.kir:  125K
# app.kirb:  15K  (5-10× smaller)
```

### Compile Directly to .kirb

```bash
# Compile to binary in one step
kryon compile app.nim --format=binary
# → app.kirb

# Or use explicit extension
kryon compile app.nim --output=app.kirb
# → app.kirb
```

### Validate IR Files

```bash
# Validate JSON format
kryon validate app.kir

# Validate binary format
kryon validate app.kirb
```

### Run Application

```bash
# Run from source (auto-compiles)
kryon run app.nim
# → Compiles to .kir → .kirb → Runs

# Run from .kir (auto-compiles to .kirb)
kryon run app.kir
# → Converts to .kirb → Runs

# Run from .kirb (direct load)
kryon run app.kirb
# → Runs directly
```

## Best Practices

1. **Frontends output .kir** - Always use JSON for frontend output
2. **Convert to .kirb for production** - Binary format for deployment
3. **Test each module independently** - Use modular test structure
4. **Validate before deploying** - Run `kryon validate` on all IR files
5. **Use auto-compilation** - Let the IR layer handle .kir → .kirb conversion
6. **Keep .kir for debugging** - JSON format is easier to inspect
7. **Version your IR files** - Track IR format changes in version control

## Troubleshooting

### Backend can't load .kir file

**Problem:** Backend receives .kir but expects .kirb

**Solution:** Enable auto-compilation in backend or manually convert:
```bash
kryon convert app.kir app.kirb
```

### Frontend outputs .kirb instead of .kir

**Problem:** Frontend violates standardization rules

**Solution:** Use `ir_write_json_v2_file()` instead of `ir_write_binary_file()`

### .kir format differs between frontends

**Problem:** Frontend adds custom properties/extensions

**Solution:** Use only standard `ir_builder` API, no frontend-specific extensions

### Roundtrip test fails

**Problem:** .kir → IR → .kir produces different output

**Solution:** Check property serialization, ensure all fields are preserved

## Summary

The Kryon IR pipeline is a **modular, testable architecture** that enables:

✅ **Any frontend → .kir (JSON)** - Standardized, human-readable format
✅ **IR layer → .kirb (binary)** - Automatic optimization
✅ **Backends only read .kirb** - Fast, efficient loading
✅ **5 independent modules** - Frontend, Serialization, Validation, Binary Compilation, Backend
✅ **Comprehensive testing** - Unit tests, integration tests, roundtrip tests
✅ **Clear CLI tools** - compile, convert, validate, run

**One IR format, multiple frontends, multiple backends!**
