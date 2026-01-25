# Deep Refactoring Plan

## Current Status (2026-01-24)

### Completed Extractions

| File | Original | Current | Reduction | Modules |
|------|----------|---------|-----------|---------|
| ir_c_codegen.c | 2,907 | 712 | 75% | 8 modules |
| kry_to_ir.c | 5,013 | 3,033 | 40% | 4 modules |
| kry_parser.c | 3,122 | 3,122 | 0% | 4 modules |

### Extracted Modules (Working)
- **ir_c_codegen**: ir_c_output, ir_c_types, ir_c_expression, ir_c_reactive, ir_c_components, ir_c_main, ir_c_modules
- **kry_to_ir**: kry_to_ir_context, kry_to_ir_expressions, kry_to_ir_properties, kry_to_ir_functions
- **kry_parser**: kry_allocator, kry_lexer, kry_ast_factory, kry_tokens

---

## Target Module Size: 200-400 lines each

---

## File 1: kry_to_ir.c (4,711 lines → ~300 lines orchestrator)

### Current Structure Analysis

| Section | Lines | Functions | Description |
|---------|-------|-----------|-------------|
| Property Handlers | 205-1315 | 35 | handle_*_property functions |
| Property Dispatch | 1318-1413 | 2 | dispatch_property, apply_property |
| Forward Decls | 1415-1425 | 5 | convert_node, expand_*, clone_* |
| Loop Expansion | 1426-1683 | 1 | expand_for_loop (258 lines!) |
| Function Stmts | 1690-2020 | 5 | convert_function_stmt, return, etc |
| ForEach Convert | 2022-2112 | 2 | convert_for_each_node |
| **convert_node** | 2113-3050 | 1 | **937 lines - MUST SPLIT** |
| Component Clone | 3051-3234 | 2 | clone_tree, merge_trees |
| Custom Components | 3235-3595 | 4 | is_custom, expand_template, extract_* |
| Import System | 3596-3927 | 7 | resolve_path, import stack, load_module |
| Main Entry | 3934-4711 | 1 | ir_kry_to_kir_internal_ex (777 lines!) |

### Proposed Module Structure

```
ir/parsers/kry/
├── kry_to_ir.c                    (~300 lines - orchestrator only)
├── kry_to_ir_internal.h           (shared context, already exists)
│
├── kry_to_ir_properties.h/c       (~400 lines)
│   ├── Property handler registry/dispatch table
│   ├── dispatch_property()
│   ├── apply_property()
│   └── parse_color_value(), parse_alignment()
│
├── kry_to_ir_handlers_layout.c    (~350 lines)
│   ├── handle_width/height/min/max properties
│   ├── handle_padding/gap
│   ├── handle_posX/posY
│   └── handle_contentAlignment, justifyContent
│
├── kry_to_ir_handlers_style.c     (~300 lines)
│   ├── handle_backgroundColor/color/borderColor
│   ├── handle_borderWidth/borderRadius
│   ├── handle_fontSize/fontWeight/fontFamily
│   └── handle_className
│
├── kry_to_ir_handlers_content.c   (~200 lines)
│   ├── handle_text/label
│   ├── handle_checked
│   ├── handle_selectedIndex/options
│   └── handle_windowTitle/Width/Height
│
├── kry_to_ir_handlers_events.c    (~150 lines)
│   ├── create_event_handler()
│   ├── handle_onClick/onHover/onChange
│   └── Event registration logic
│
├── kry_to_ir_expressions.h/c      (~320 lines - DONE)
│
├── kry_to_ir_context.h/c          (~200 lines - DONE)
│
├── kry_to_ir_functions.h/c        (~350 lines)
│   ├── convert_function_stmt()
│   ├── convert_child_statements()
│   ├── convert_return_stmt()
│   ├── convert_module_return()
│   └── convert_function_decl()
│
├── kry_to_ir_loops.h/c            (~400 lines)
│   ├── expand_for_loop() - 258 lines
│   ├── convert_for_each_node() - 90 lines
│   └── is_variable_reference()
│
├── kry_to_ir_components.h/c       (~450 lines)
│   ├── ir_component_clone_tree()
│   ├── merge_component_trees()
│   ├── is_custom_component()
│   ├── expand_component_template()
│   ├── extract_component_props()
│   └── extract_component_state_vars()
│
├── kry_to_ir_imports.h/c          (~350 lines)
│   ├── resolve_module_path()
│   ├── Import stack (push/pop/reset)
│   ├── is_module_being_loaded()
│   ├── register_module_import()
│   └── load_imported_module()
│
├── kry_to_ir_convert.h/c          (~500 lines)
│   ├── get_component_type()
│   ├── resolve_value_as_string()
│   ├── get_value_as_bool()
│   └── convert_node() - SPLIT INTO CASES
│
└── kry_to_ir_kir.h/c              (~400 lines)
    └── ir_kry_to_kir_internal_ex() - SPLIT INTO PHASES
```

### convert_node() Split Strategy (937 lines → 3 modules)

The monster `convert_node()` function handles too many cases:

```c
// Current structure (simplified):
switch (node->type) {
    case KRY_NODE_COMPONENT:      // ~400 lines
    case KRY_NODE_VAR_DECL:       // ~50 lines
    case KRY_NODE_FUNCTION_DECL:  // ~30 lines
    case KRY_NODE_FOR_LOOP:       // ~30 lines
    case KRY_NODE_FOR_EACH:       // ~20 lines
    case KRY_NODE_IF:             // ~100 lines
    case KRY_NODE_CODE_BLOCK:     // ~150 lines
    case KRY_NODE_RETURN_STMT:    // ~50 lines
    case KRY_NODE_IMPORT:         // ~100 lines
    ...
}
```

**Split into:**
1. `kry_to_ir_convert_component.c` - KRY_NODE_COMPONENT case (~400 lines)
2. `kry_to_ir_convert_control.c` - IF, FOR, FOREACH cases (~150 lines)
3. `kry_to_ir_convert_misc.c` - VAR_DECL, CODE_BLOCK, IMPORT, etc (~200 lines)
4. `kry_to_ir_convert.c` - Dispatcher that calls the above (~100 lines)

---

## File 2: kry_parser.c (3,122 lines → ~300 lines orchestrator)

### Current Structure Analysis

| Section | Lines | Functions | Description |
|---------|-------|-----------|-------------|
| Helpers | 1-200 | 10 | skip_*, peek_*, match, checkpoint |
| Value Parsing | 200-700 | 8 | parse_value (277 lines!), parse_array, parse_object |
| Expression | 700-1000 | 3 | parse_expression_text (160 lines), parse_arrow_function |
| Property/Assign | 1000-1435 | 5 | parse_property, parse_*_assignment |
| Forward Decls | 1574-1590 | 12 | Statement parser declarations |
| Keyword Dispatch | 1590-1690 | 12 | handle_* wrappers, dispatch table |
| Var Declaration | 1695-1760 | 1 | parse_var_decl |
| Import Statement | 1760-1890 | 1 | parse_import_statement (130 lines) |
| Component Body | 1892-2045 | 2 | parse_component_body, parse_code_block |
| For Loop | 2046-2085 | 1 | parse_for_loop |
| If Statement | 2088-2202 | 1 | parse_if_statement (115 lines) |
| Function Decl | 2205-2404 | 2 | parse_function_declaration (200 lines!) |
| Return/Delete | 2407-2520 | 2 | parse_return_statement, parse_delete_statement |
| Static Block | 2520-2600 | 1 | parse_static_block |
| Style Block | 2600-2700 | 1 | parse_style_block |
| Module Return | 2700-2800 | 1 | parse_module_return |
| Main Parse | 2800-3122 | 2 | kry_parse (228 lines), helpers |

### Proposed Module Structure

```
ir/parsers/kry/
├── kry_parser.c                   (~300 lines - orchestrator)
├── kry_parser_internal.h          (shared state, forward decls)
│
├── kry_allocator.h/c              (~50 lines - DONE)
├── kry_lexer.h/c                  (~120 lines - DONE)
├── kry_ast_factory.h/c            (~170 lines - DONE)
├── kry_tokens.h/c                 (~110 lines - DONE)
│
├── kry_parser_values.h/c          (~400 lines)
│   ├── parse_value() - THE CORE (277 lines)
│   ├── parse_array()
│   ├── parse_object()
│   ├── parse_struct_instantiation()
│   └── parse_complex_expression()
│
├── kry_parser_expressions.h/c     (~350 lines)
│   ├── parse_expression_text() - 160 lines
│   ├── parse_arrow_function_text()
│   ├── peek_is_binary_operator()
│   └── Expression buffer helpers
│
├── kry_parser_properties.h/c      (~250 lines)
│   ├── parse_property()
│   ├── parse_property_access_assignment()
│   ├── parse_indexed_assignment()
│   └── Property parsing helpers
│
├── kry_parser_statements.h/c      (~400 lines)
│   ├── parse_var_decl()
│   ├── parse_for_loop()
│   ├── parse_if_statement() - 115 lines
│   ├── parse_return_statement()
│   ├── parse_delete_statement()
│   └── parse_static_block()
│
├── kry_parser_functions.h/c       (~250 lines)
│   ├── parse_function_declaration() - 200 lines
│   └── Function parameter parsing
│
├── kry_parser_imports.h/c         (~200 lines)
│   ├── parse_import_statement() - 130 lines
│   └── Module path resolution
│
├── kry_parser_components.h/c      (~300 lines)
│   ├── parse_component_body()
│   ├── parse_code_block()
│   ├── parse_style_block()
│   └── parse_module_return()
│
└── kry_parser_keywords.h/c        (~150 lines)
    ├── Keyword dispatch table
    ├── handle_* wrapper functions
    └── dispatch_keyword()
```

---

## File 3: ir_c_codegen.c (712 lines → ~200 lines)

### Current State (Already Good)
Most extraction done. Remaining 712 lines contain:
- Header generation functions (~400 lines)
- Entry point ir_generate_c_code_multi (~300 lines)

### Final Module Structure

```
codegens/c/
├── ir_c_codegen.c                 (~200 lines - entry points only)
│
├── ir_c_headers.h/c               (~400 lines) ← NEW
│   ├── generate_includes()
│   ├── generate_preprocessor_directives()
│   ├── generate_variable_declarations()
│   ├── generate_helper_functions()
│   └── generate_event_handlers()
│
├── ir_c_output.h/c                (~20 lines - DONE)
├── ir_c_types.h/c                 (~80 lines - DONE)
├── ir_c_expression.h/c            (~640 lines - DONE)
├── ir_c_reactive.h/c              (~220 lines - DONE)
├── ir_c_components.h/c            (~860 lines - DONE)
├── ir_c_main.h/c                  (~280 lines - DONE)
└── ir_c_modules.h/c               (~160 lines - DONE)
```

---

## Implementation Priority

### Phase 1: High Impact (Do First)
1. **kry_to_ir_properties.c** - Extract 35 property handlers (~1100 lines out)
2. **kry_parser_values.c** - Extract parse_value and related (~500 lines out)
3. **ir_c_headers.c** - Extract header generation (~400 lines out)

### Phase 2: Medium Impact
4. **kry_to_ir_imports.c** - Extract import system (~350 lines)
5. **kry_to_ir_loops.c** - Extract loop expansion (~350 lines)
6. **kry_parser_statements.c** - Extract statement parsers (~400 lines)

### Phase 3: Core Refactoring
7. **Split convert_node()** - Break into 3-4 modules
8. **Split ir_kry_to_kir_internal_ex()** - Break into phases
9. **kry_to_ir_components.c** - Extract template expansion

### Phase 4: Cleanup
10. Extract remaining small modules
11. Create proper header hierarchy
12. Update all Makefiles

---

## Expected Final State

| File | Current | Target | Reduction |
|------|---------|--------|-----------|
| kry_to_ir.c | 4,711 | ~300 | 94% |
| kry_parser.c | 3,122 | ~300 | 90% |
| ir_c_codegen.c | 712 | ~200 | 72% |
| **Total** | **8,545** | **~800** | **91%** |

### New Modules Count
- kry_to_ir: 2 existing + 12 new = 14 modules
- kry_parser: 4 existing + 8 new = 12 modules
- ir_c_codegen: 7 existing + 1 new = 8 modules
- **Total: 34 focused modules**

---

## Key Principles

1. **Each module ≤ 400 lines** - Easier to understand and maintain
2. **Single responsibility** - One module = one concern
3. **Clear dependencies** - Header files define interfaces
4. **Testable** - Each module can be unit tested independently
5. **No circular deps** - Layered architecture with clear hierarchy
