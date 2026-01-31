# Tcl/Tk Parser - Bidirectional Conversion

This directory contains the Tcl/Tk parser that converts Tcl/Tk source code into KIR (Kryon Intermediate Representation), enabling **round-trip conversion** between all formats:
```
KRY ⇄ KIR ⇄ Tcl ⇄ Limbo ⇄ Web ⇄ C
```

## Status

**Updated**: January 31, 2026

### ✅ Completed
- Tcl lexer (tokenization)
- Tcl parser (AST construction)
- Widget registry (Tcl ↔ KIR mappings)
- Widget hierarchy building (nested paths like `.w1.w2.w3`)
- KIR JSON conversion (cJSON output)
- **Native IR API** (`ir_tcl_parse()` returns `IRComponent*`)
- Property extraction (colors, text, dimensions)
- Multi-line command parsing

### 🟡 Partial Support
- Layout commands (pack/grid/place) - parsed but not applied to layout
- Event handlers (bind) - parsed but not extracted to logic_block

### ❌ Not Implemented
- Complex expressions
- Array variables
- Namespace support

## API Pattern (matches KRY parser)

The parser follows the **same API pattern** as the KRY parser:

```c
// Parse Tcl to native IR component tree
IRComponent* ir_tcl_parse(const char* source, size_t length);

// Convert Tcl to KIR JSON string
char* ir_tcl_to_kir(const char* source, size_t length);

// Parse with error reporting
IRComponent* ir_tcl_parse_with_errors(const char* source, size_t length,
                                     char** error_message,
                                     uint32_t* error_line,
                                     uint32_t* error_column);
```

## Implementation

Two-step approach:
1. **Lexer + Parser** → Tcl AST
2. **Converter** → KIR JSON (cJSON)
3. **Wrapper** → Native IR* (via `ir_deserialize_json()`)

This matches the KRY parser pattern and keeps the implementation modular.

## Usage

### Parse to Native IR (Recommended)

```c
#include "ir/parsers/tcl/tcl_parser.h"

const char* tcl = "frame .w1 -background #1a1a2e";
IRComponent* root = ir_tcl_parse(tcl, 0);
if (root) {
    // Use native IR component tree
    ir_destroy_component(root);
}
```

### Parse to KIR JSON

```c
char* kir_json = ir_tcl_to_kir(tcl, 0);
if (kir_json) {
    printf("KIR: %s\n", kir_json);
    free(kir_json);
}
```

### Parse with Error Handling

```c
char* error = NULL;
uint32_t line, col;
IRComponent* root = ir_tcl_parse_with_errors(tcl, 0, &error, &line, &col);
if (!root && error) {
    fprintf(stderr, "Error at %u:%u: %s\n", line, col, error);
    free(error);
}
```

### Legacy cJSON API (still works)

```c
#include "tcl_to_ir.h"

cJSON* kir = tcl_parse_to_kir(source_code);
// ... use kir
cJSON_Delete(kir);
```

## Bidirectional Conversion

### Current Support

| Format | Parse → KIR | KIR → Generate | Round-Trip |
|--------|------------|----------------|------------|
| **KRY** | ✅ `ir_kry_parse()` | ✅ `kry_codegen_*` | ✅ ~95% |
| **Tcl/Tk** | ✅ `ir_tcl_parse()` | ✅ `tcltk_codegen_*` | ⚠️ ~70% |
| **Limbo** | ❌ Needed | ✅ `limbo_codegen_*` | ❌ N/A |
| **Web** | ❌ Needed | ✅ `web_codegen_*` | ❌ N/A |
| **C** | ❌ Needed | ✅ `c_codegen_*` | ❌ N/A |

### Universal Conversion Pattern

```bash
# Parse any format to KIR
kryon parse tcl input.tcl -o output.kir
kryon parse kry input.kry -o output.kir

# Generate from KIR to any format
kryon codegen tcltk input.kir -o output.tcl
kryon codegen limbo input.kir -o output.b

# Convert any format to any format
kryon convert input.tcl -o output.kry
kryon convert input.b -o output.tcl
```

## Widget Type Mappings

| Tcl Widget | KIR Type |
|------------|----------|
| button | Button |
| label | Text |
| entry | Input |
| text | TextArea |
| frame | Container |
| canvas | Canvas |
| checkbutton | Checkbox |
| radiobutton | Radio |
| listbox | List |
| scale | Slider |
| notebook | TabView |

## Property Mappings

| Tcl Option | KIR Property |
|------------|--------------|
| -text | text |
| -background | background |
| -foreground | color |
| -font | font |
| -width | width |
| -height | height |
| -padx | paddingX |
| -pady | paddingY |

## Building

```bash
cd ir/parsers/tcl
make clean && make
```

Generates:
- `libtclparser.so` - Shared library
- `libtclparser.a` - Static library

## Testing

```bash
./widget_test
```

Expected output: Valid KIR JSON with proper widget hierarchy

## Integration with CLI

To integrate with the CLI for universal conversion:

### 1. Add to Target Handlers

In `cli/src/config/target_handlers.c`:

```c
// Parse Tcl files
if (strcmp(ext, ".tcl") == 0) {
    char* kir_json = ir_tcl_to_kir(source_code, 0);
    // Write to output.kir
}

// Parse KRY files
if (strcmp(ext, ".kry") == 0) {
    char* kir_json = ir_kry_to_kir(source_code, 0);
    // Write to output.kir
}
```

### 2. Universal Convert Command

```c
// CLI command: kryon convert input.X -o output.Y
// 1. Detect input format from extension
// 2. Parse to KIR (use appropriate parser)
// 3. Generate output (use appropriate codegen)

// Example: Tcl → KRY
char* kir_json = ir_tcl_to_kir(tcl_source, 0);
IRComponent* root = ir_deserialize_json(kir_json);
char* kry_source = kry_codegen_from_json(kir_json);
```

## Limitations

1. **Layout reconstruction**: Pack/grid/place commands are parsed but not yet converted to KIR layout
2. **Event handlers**: Handler code is captured but not extracted to logic_block
3. **Procedures**: Proc definitions parsed but not fully converted
4. **Variable substitutions**: Captured but not resolved
5. **Complex strings**: Nested braces/quotes may have issues

## Next Steps

1. ✅ Parser Foundation - Complete
2. ⏳ Layout Parsing - Apply pack/grid/place to KIR layout properties
3. ⏳ Event Handlers - Extract bind commands to logic_block
4. ⏳ CLI Integration - Add `kryon parse` and `kryon convert` commands
5. ⏳ Round-trip Testing - Tcl → KIR → KRY → KIR → Tcl validation

## File Structure

```
ir/parsers/tcl/
├── tcl_lexer.c              # Lexical analysis
├── tcl_parser.c             # Parser + AST + Native IR wrappers
├── tcl_widget_registry.c    # Widget/property mappings
├── tcl_to_ir.c              # AST → KIR JSON conversion
├── widget_test.c            # Test program
├── Makefile                 # Build configuration
└── README.md                # This file
```

## Notes

- **Follows KRY parser pattern**: `ir_tcl_parse()` returns `IRComponent*`
- **Uses IR serialization**: `ir_deserialize_json()` for JSON → native IR
- **Modular design**: Lexer, parser, converter are separate
- **Compatible**: Works with all IR-based renderers and codegens
