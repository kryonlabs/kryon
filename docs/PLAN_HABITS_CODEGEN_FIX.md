# Plan: Complete C Codegen Fix for Habits App

## Problem Summary

The habits app (`main.kry`) fails to compile because the C codegen generates invalid code for:
1. Struct types (Habit) - not generated
2. Property access on void* - `habit.name` invalid on `void*`
3. Local variable declarations - variables used without declaration
4. Module imports - DEFAULT_COLOR not resolved

---

## Architecture Overview

```
main.kry          types.kry         KIR Parser
    |                 |                 |
    v                 v                 v
 [KRY Parser] -----> [IR Generator] --> main.kir
                                            |
                                            v
                                    [C Codegen]
                                            |
                                            v
                                      main.c (broken)
```

**Key Files:**
- `codegens/c/ir_c_codegen.c` - Main codegen entry, function generation
- `codegens/c/ir_c_expression.c` - Expression/statement handling
- `codegens/c/ir_c_components.c` - UI component generation
- `codegens/c/ir_c_main.c` - Main function generation
- `ir/parsers/kry/` - KRY language parser

---

## Issue 1: Struct Type Generation

### Problem
The `Habit` struct defined in `types.kry` is not being generated in the C output.

### Current State
```kry
// types.kry
struct Habit {
  id = UUID.v4()
  name = "New Habit"
  createdAt = DateTime.today().toString()
  completions = {}
  color = getRandomColor()
}
```

The KIR has no struct type information - it's lost during parsing.

### Solution

#### Step 1.1: Extend KRY Parser for Structs
**File:** `ir/parsers/kry/kry_parser.c`

Add struct parsing to capture:
- Struct name
- Field names and types
- Default values

```c
// Add to parse_statement or similar
if (strcmp(keyword, "struct") == 0) {
    return parse_struct_definition(parser);
}

static ASTNode* parse_struct_definition(Parser* parser) {
    // Parse: struct Name { field = default; ... }
    // Store in source_structures.struct_types
}
```

#### Step 1.2: Serialize Structs to KIR
**File:** `ir/src/ir_json_serialize.c`

Add struct serialization:
```json
"source_structures": {
  "struct_types": [
    {
      "name": "Habit",
      "fields": [
        {"name": "id", "type": "string", "default": "UUID.v4()"},
        {"name": "name", "type": "string", "default": "\"New Habit\""},
        {"name": "createdAt", "type": "string", "default": "DateTime.today().toString()"},
        {"name": "completions", "type": "object", "default": "{}"},
        {"name": "color", "type": "string", "default": "getRandomColor()"}
      ]
    }
  ]
}
```

#### Step 1.3: Generate C Structs
**File:** `codegens/c/ir_c_codegen.c`

Add function to generate C struct definitions:
```c
void generate_struct_definitions(CCodegenContext* ctx) {
    cJSON* structs = cJSON_GetObjectItem(ctx->source_structures, "struct_types");
    if (!structs) return;

    cJSON* s;
    cJSON_ArrayForEach(s, structs) {
        const char* name = cJSON_GetObjectItem(s, "name")->valuestring;
        cJSON* fields = cJSON_GetObjectItem(s, "fields");

        fprintf(ctx->output, "typedef struct {\n");
        cJSON* f;
        cJSON_ArrayForEach(f, fields) {
            const char* fname = cJSON_GetObjectItem(f, "name")->valuestring;
            const char* ftype = cJSON_GetObjectItem(f, "type")->valuestring;
            fprintf(ctx->output, "    %s %s;\n", map_type_to_c(ftype), fname);
        }
        fprintf(ctx->output, "} %s;\n\n", name);
    }
}
```

---

## Issue 2: Property Access on Typed Pointers

### Problem
Generated code uses `habit.name` but `habit` is `void*`.

### Current Generated Code
```c
FOR_EACH_TYPED(void*, habit, habits, habits_count,
    TAB(..., BIND_TEXT_EXPR(habit.name), ...)
);
```

### Solution

#### Step 2.1: Track Array Element Types
**File:** `codegens/c/ir_c_codegen.c`

When processing `for_def`, extract the element type:
```c
// In for_def, source.expression = "habits"
// Look up habits declaration to find its element type
// If habits is Habit[], element type is Habit
```

Add a type registry to track variable types:
```c
typedef struct {
    char name[256];
    char type[256];        // "Habit", "int", "string", etc.
    char element_type[256]; // For arrays: element type
    bool is_array;
} VariableTypeInfo;

static VariableTypeInfo g_var_types[100];
static int g_var_type_count = 0;
```

#### Step 2.2: Generate Typed FOR_EACH
**File:** `codegens/c/ir_c_components.c`

Change FOR_EACH generation to use actual type:
```c
// Before:
FOR_EACH_TYPED(void*, habit, habits, habits_count, ...)

// After:
FOR_EACH_TYPED(Habit*, habit, habits, habits_count, ...)
```

#### Step 2.3: Fix Property Access in Expressions
**File:** `codegens/c/ir_c_expression.c`

When generating member access, check if the variable is a pointer:
```c
// In c_expr_to_c, handle member_access:
if (strcmp(op_str, "member_access") == 0) {
    char* obj = c_expr_to_c(cJSON_GetObjectItem(expr, "object"));
    const char* prop = cJSON_GetObjectItem(expr, "property")->valuestring;

    // Check if obj is a pointer type
    if (is_pointer_type(obj)) {
        // Generate: obj->prop
        sprintf(result, "%s->%s", obj, prop);
    } else {
        // Generate: obj.prop
        sprintf(result, "%s.%s", obj, prop);
    }
}
```

---

## Issue 3: Local Variable Declarations

### Problem
Functions use variables without declaring them:
```c
static void* loadHabits(void) {
    habits = Storage_load(...);  // habits not declared
    today = DateTime_today();    // today not declared
```

### Solution

#### Step 3.1: Track Local Variables in Functions
**File:** `codegens/c/ir_c_expression.c`

Add local variable tracking per function:
```c
typedef struct {
    char name[256];
    char type[256];
    bool declared;
} LocalVariable;

static LocalVariable g_local_vars[50];
static int g_local_var_count = 0;

void reset_local_vars(void) {
    g_local_var_count = 0;
}

bool is_local_var_declared(const char* name) {
    for (int i = 0; i < g_local_var_count; i++) {
        if (strcmp(g_local_vars[i].name, name) == 0) {
            return g_local_vars[i].declared;
        }
    }
    return false;
}
```

#### Step 3.2: Generate Declaration on First Assignment
**File:** `codegens/c/ir_c_expression.c`

In `c_stmt_to_c` for `assign`:
```c
if (strcmp(op_str, "assign") == 0) {
    const char* target = cJSON_GetObjectItem(stmt, "target")->valuestring;

    // Check if this is a global variable
    if (is_global_var(target)) {
        // Just assign, it's already declared
        fprintf(output, "    %s = %s;\n", target, expr_c);
    }
    // Check if local var needs declaration
    else if (!is_local_var_declared(target)) {
        // Infer type from expression
        const char* type = infer_type_from_expr(expr);
        fprintf(output, "    %s %s = %s;\n", type, target, expr_c);
        mark_local_var_declared(target, type);
    } else {
        // Already declared, just assign
        fprintf(output, "    %s = %s;\n", target, expr_c);
    }
}
```

#### Step 3.3: Reset Local Vars Between Functions
**File:** `codegens/c/ir_c_codegen.c`

In `generate_universal_functions`:
```c
cJSON_ArrayForEach(func, functions) {
    reset_local_vars();  // Clear local variable tracking
    // ... generate function ...
}
```

---

## Issue 4: Module Import Resolution

### Problem
`DEFAULT_COLOR` imported from `components.color_palette` is undefined.

### Solution

#### Step 4.1: Parse and Store Imports in KIR
**File:** `ir/parsers/kry/kry_parser.c`

The imports are already in KIR under `source_structures.requires`:
```json
{
  "variable": "COLORS,DEFAULT_COLOR,getRandomColor",
  "module": "components.color_palette"
}
```

#### Step 4.2: Resolve Module Exports
**File:** `codegens/c/ir_c_modules.c`

Create module resolution:
```c
// Load the imported module's KIR
// Find the exported constants/functions
// Generate #define or extern declarations

void resolve_module_imports(CCodegenContext* ctx) {
    cJSON* requires = cJSON_GetObjectItem(ctx->source_structures, "requires");

    cJSON* req;
    cJSON_ArrayForEach(req, requires) {
        const char* vars = cJSON_GetObjectItem(req, "variable")->valuestring;
        const char* module = cJSON_GetObjectItem(req, "module")->valuestring;

        // Load module KIR (e.g., components/color_palette.kir)
        // Extract exported values
        // Generate declarations
    }
}
```

#### Step 4.3: Generate Constant Definitions
For simple constants like `DEFAULT_COLOR`:
```c
// If color_palette exports DEFAULT_COLOR = "#3498db"
fprintf(ctx->output, "#define DEFAULT_COLOR \"#3498db\"\n");
```

---

## Issue 5: Broken Syntax in Object Literals

### Problem
```c
habit.completions = };  // Invalid syntax
```

This comes from `"expr": {"var": "}"}` in the KIR which is a parser bug.

### Solution

#### Step 5.1: Fix KRY Parser Object Literal Handling
**File:** `ir/parsers/kry/kry_parser.c`

The issue is parsing `habit.completions = {}` - the `{}` is being parsed incorrectly.

```c
// In parse_expression, handle empty object literal
if (current_char == '{' && peek_char == '}') {
    // Return proper object_literal node
    return create_object_literal_node(NULL);  // empty properties
}
```

#### Step 5.2: Generate Proper C for Object Literals
**File:** `codegens/c/ir_c_expression.c`

```c
// For object_literal with no properties:
if (strcmp(op_str, "object_literal") == 0) {
    cJSON* props = cJSON_GetObjectItem(expr, "properties");
    if (!props || cJSON_GetArraySize(props) == 0) {
        return strdup("NULL");  // Empty object = NULL in C
    }
    // Handle non-empty objects...
}
```

---

## Implementation Order

### Phase 1: Core Type System (Priority: High)
1. [ ] **1.1** Parse struct definitions in KRY parser
2. [ ] **1.2** Serialize structs to KIR JSON
3. [ ] **1.3** Generate C struct definitions in codegen
4. [ ] **5.1** Fix object literal parsing bug

### Phase 2: Expression Handling (Priority: High)
5. [ ] **3.1** Add local variable tracking
6. [ ] **3.2** Generate declarations on first assignment
7. [ ] **3.3** Reset tracking between functions
8. [ ] **5.2** Generate proper C for object literals

### Phase 3: Type-Aware Code Generation (Priority: Medium)
9. [ ] **2.1** Track array element types
10. [ ] **2.2** Generate typed FOR_EACH
11. [ ] **2.3** Fix property access for pointers

### Phase 4: Module System (Priority: Medium)
12. [ ] **4.1** Parse module exports properly
13. [ ] **4.2** Resolve module imports
14. [ ] **4.3** Generate constant definitions

---

## Testing Strategy

### Unit Tests
- Struct parsing: `test_parse_struct_definition()`
- Type inference: `test_infer_type_from_expr()`
- Local var tracking: `test_local_var_declaration()`

### Integration Tests
Create minimal test files:
```
tests/c_codegen/
├── struct_basic.kry      # Basic struct definition
├── struct_array.kry      # Array of structs
├── for_each_typed.kry    # Typed iteration
├── module_import.kry     # Import resolution
└── function_vars.kry     # Local variable handling
```

### End-to-End Test
The habits app itself serves as the comprehensive test.

---

## Files to Modify

| File | Changes |
|------|---------|
| `ir/parsers/kry/kry_parser.c` | Add struct parsing, fix object literal |
| `ir/src/ir_json_serialize.c` | Serialize struct types |
| `codegens/c/ir_c_codegen.c` | Generate structs, type tracking |
| `codegens/c/ir_c_expression.c` | Local vars, property access, object literals |
| `codegens/c/ir_c_components.c` | Typed FOR_EACH |
| `codegens/c/ir_c_modules.c` | Module import resolution |
| `codegens/c/ir_c_internal.h` | Type tracking structures |

---

## Success Criteria

1. `kryon build` on habits app produces no compilation errors
2. Generated `main.c` contains:
   - `typedef struct { ... } Habit;`
   - `Habit* habits_array` (typed array)
   - `FOR_EACH_TYPED(Habit*, habit, ...)` (typed iteration)
   - Proper local variable declarations
   - `#define DEFAULT_COLOR "..."`
3. Binary runs and displays habit tabs

---

## Estimated Complexity

| Phase | Effort | Risk |
|-------|--------|------|
| Phase 1 | High | Medium - Parser changes are delicate |
| Phase 2 | Medium | Low - Well-defined scope |
| Phase 3 | Medium | Medium - Type inference can be tricky |
| Phase 4 | Low | Low - Straightforward resolution |

**Total estimated effort:** 3-4 focused coding sessions
