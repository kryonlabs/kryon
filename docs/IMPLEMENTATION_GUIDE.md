# Kryon Implementation Guide

Quick reference for extending Kryon.

## Adding a Parser

Create `ir/parsers/mylang/mylang_parser.c`:

```c
cJSON* parse_mylang_to_kir_json(const char* source) {
    cJSON* kir = cJSON_CreateObject();
    cJSON_AddStringToObject(kir, "format", "kir");

    cJSON* metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "source_language", "mylang");
    cJSON_AddItemToObject(kir, "metadata", metadata);

    // Build root component tree
    cJSON* root = ...; // Parse source
    cJSON_AddItemToObject(kir, "root", root);

    return kir;
}
```

Add to CLI in `cli/src/commands/cmd_compile.c`:

```c
#include "parsers/mylang/mylang_parser.h"

if (strcmp(frontend, "mylang") == 0) {
    cJSON* kir = parse_mylang_to_kir_json(source);
    // Write to file
}
```

## Adding a Codegen

Create `codegens/mytarget/mytarget_codegen.c`:

```c
bool mytarget_codegen_generate(const char* kir_path, const char* output_path) {
    // Read KIR file
    cJSON* kir = cJSON_Parse(content);
    
    // Open output
    FILE* out = fopen(output_path, "w");
    
    // Generate code from KIR
    cJSON* root = cJSON_GetObjectItem(kir, "root");
    generate_component(out, root);
    
    fclose(out);
    cJSON_Delete(kir);
    return true;
}
```

Add to CLI in `cli/src/commands/cmd_codegen.c`:

```c
#include "../../codegens/mytarget/mytarget_codegen.h"

else if (strcmp(language, "mytarget") == 0) {
    success = mytarget_codegen_generate(input_kir, output_file);
}
```

Update `cli/Makefile`:

```makefile
CODEGEN_OBJS = ... ../codegens/mytarget/mytarget_codegen.o
```

## Adding a Component

1. Add to TSX parser component list in `ir/parsers/tsx/tsx_to_kir.ts`
2. Add rendering in `codegens/react_common.c`
3. Add desktop rendering in `backends/desktop/`
4. Test roundtrip

## Adding Events

Events auto-detect from `on*` props in TSX.

Add to React codegen event mapping in `codegens/react_common.c`:

```c
if (strcmp(evt_type, "myevent") == 0) {
    snprintf(output, output_size, "onMyEvent");
}
```

Add to desktop backend in `backends/desktop/desktop_input.c` to handle the SDL event.

## Adding Universal Logic

In `ir/parsers/tsx/tsx_to_kir.ts`, add to `convertToUniversalLogic()`:

```typescript
// Pattern: setCount(count * 2)
if (value === `${varName} * 2`) {
  return {
    params: [],
    statements: [{
      type: "assign",
      target: varName,
      expr: {
        op: "mul",
        left: {"var": varName},
        right: {"value": 2, "type": "int"}
      }
    }]
  };
}
```

Add operator to executor in `ir/ir_executor.c`:

```c
case EXPR_OP_MUL:
    return ir_value_int(left.int_val * right.int_val);
```

## Testing

### Round-Trip

```bash
# Source → KIR → Source
./cli/kryon compile input.mylang > test.kir
./cli/kryon codegen mylang test.kir output.mylang
diff input.mylang output.mylang
```

### Full Pipeline

```bash
bun ir/parsers/tsx/tsx_to_kir.ts app.tsx > app.kir
./cli/kryon codegen c app.kir app.c
gcc app.c -lkryon_desktop -o app
./app
```

## Code Style

### C

- Linux kernel style
- snake_case for functions/variables
- PascalCase for types
- Max ~100 char lines

### TypeScript

- Strict mode
- camelCase for functions/variables
- PascalCase for types/interfaces

## Memory Management

```c
// Always free
char* str = strdup(input);
free(str);

// cJSON
cJSON* json = cJSON_Parse(input);
cJSON_Delete(json);

// IRComponent
IRComponent* comp = ir_component_create("Text");
ir_component_free(comp);
```

## Common Patterns

### Traverse Component Tree

```c
void traverse(cJSON* comp) {
    // Process component
    
    cJSON* children = cJSON_GetObjectItem(comp, "children");
    if (children && cJSON_IsArray(children)) {
        for (int i = 0; i < cJSON_GetArraySize(children); i++) {
            traverse(cJSON_GetArrayItem(children, i));
        }
    }
}
```

### String Building

```c
StringBuilder* sb = sb_create(256);
sb_append(sb, "Hello");
sb_append_fmt(sb, " %s", name);
char* result = strdup(sb_get(sb));
sb_free(sb);
```

---

For detailed examples, see existing parsers/codegens in the codebase.
