# Tcl/Tk Parser

This directory contains the Tcl/Tk parser that converts Tcl/Tk source code into KIR (Kryon Intermediate Representation), enabling round-trip conversion between KRY and Tcl/Tk.

## Status

**Implementation Phase**: Foundation Complete (January 31, 2026)

### ✅ Completed
- Tcl lexer (tokenization of Tcl source code)
- Tcl parser (AST construction)
- Widget registry (Tcl ↔ KIR type mappings)
- Property mappings (Tcl options ↔ KIR properties)
- Basic KIR conversion framework
- Test suite (passes successfully)

### 🟡 In Progress
- Full widget hierarchy building
- Complete pack/grid/place layout parsing
- Event handler extraction
- Round-trip validation

### Architecture

### Components

1. **tcl_lexer.c** - Lexical analysis (tokenization)
   - Converts Tcl source code into tokens
   - Handles strings, variables, command substitution, comments

2. **tcl_parser.c** - Parsing
   - Builds Abstract Syntax Tree (AST) from tokens
   - Identifies commands, widgets, layout statements

3. **tcl_widget_registry.c** - Widget type mapping
   - Maps Tcl widget types to KIR widget types
   - Maps Tcl options to KIR properties
   - Provides widget capability queries

4. **tcl_to_ir.c** - KIR conversion
   - Converts Tcl AST to KIR JSON format
   - Builds widget hierarchy
   - Preserves layout and styling information

## Usage

### Basic parsing

```c
#include "tcl_parser.h"

// Parse Tcl source to KIR
cJSON* kir = tcl_parse_to_kir(source_code);
if (kir) {
    char* kir_string = cJSON_Print(kir);
    printf("%s\n", kir_string);
    free(kir_string);
    cJSON_Delete(kir);
}
```

### Parse file

```c
cJSON* kir = tcl_parse_file_to_kir("app.tcl");
// ... use kir
cJSON_Delete(kir);
```

### Validate syntax

```c
if (tcl_validate_syntax(source_code)) {
    printf("Syntax is valid\n");
}
```

### Extract widget tree

```c
cJSON* tree = tcl_extract_widget_tree(source_code);
// Returns array of widgets with their properties
cJSON_Delete(tree);
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
make
make install
```

## Testing

```bash
make test
```

## Round-trip

The parser enables round-trip conversion:
```
KRY → KIR → Tcl → KIR → KRY
```

Expected preservation: ~70%

### What's preserved:
- Widget types
- Widget hierarchy (parent-child relationships)
- Most properties (text, colors, dimensions)
- Layout information (pack/grid/place)

### What's lost:
- Original formatting/indentation
- Comments (unless preserved in metadata)
- Complex event handler logic
- Variable substitutions
- Some edge case properties

## Error Handling

```c
if (!tcl_parse_to_kir(source)) {
    const char* error = tcl_parse_get_last_error();
    TclParseError code = tcl_parse_get_last_error_code();
    printf("Error %d: %s\n", code, error);
}
```

## Limitations

1. **Layout reconstruction**: Pack/grid/place commands are parsed but full layout reconstruction is complex
2. **Event handlers**: Handler code is captured but not fully analyzed
3. **Procedures**: Proc definitions are parsed but not fully converted to KIR logic blocks
4. **Variables**: Variable substitutions are captured but not resolved
5. **Complex strings**: Nested braces and quotes may not always parse correctly

## Future Improvements

1. Full layout reconstruction from pack/grid commands
2. Event handler extraction to logic_block
3. Variable resolution and tracking
4. Support for more Tcl/Tk widgets
5. Better handling of ttk:: themed widgets
