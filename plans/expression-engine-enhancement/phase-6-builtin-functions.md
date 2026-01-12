# Phase 6: Builtin Function Library

**Duration**: 4 days
**Dependencies**: Phase 4
**Status**: Pending

## Overview

Phase 6 implements a comprehensive builtin function library for the expression engine. This provides developers with useful functions they can call from expressions and event handlers.

## Objectives

1. Design extensible builtin function registry
2. Implement string manipulation functions
3. Implement array manipulation functions
4. Implement math functions
5. Implement type conversion functions
6. Add documentation for all builtins

---

## Builtin Registry Architecture

### 1.1 Function Registry

**File**: `ir/ir_builtin_registry.c`

```c
// Builtin function signature
typedef IRValue (*IRBuiltinFunc)(IREvalContext* ctx, IRValue* args, uint32_t arg_count);

// Builtin function metadata
typedef struct {
    const char* name;           // Function name
    const char* signature;      // Signature string (e.g., "(str: string) -> string")
    const char* description;    // Human-readable description
    IRBuiltinFunc func;         // Function pointer

    uint32_t min_args;          // Minimum argument count
    uint32_t max_args;          // Maximum argument count (0 = unlimited)

    // Argument types (for validation and documentation)
    VarType* arg_types;         // Expected types for each argument
    uint32_t arg_type_count;

    VarType return_type;        // Expected return type
    bool is_pure;               // Pure function (no side effects)
} IRBuiltinDef;

// Builtin registry
typedef struct {
    IRBuiltinDef* builtins;
    uint32_t count;
    uint32_t capacity;

    // Hash table for fast lookup
    IRBuiltinDef** buckets;
    uint32_t bucket_count;
} IRBuiltinRegistry;

// Global registry instance
static IRBuiltinRegistry* g_builtin_registry = NULL;
```

### 1.2 Registry Initialization

```c
// Initialize builtin registry with all standard functions
IRBuiltinRegistry* ir_builtin_registry_create(void) {
    IRBuiltinRegistry* registry = (IRBuiltinRegistry*)calloc(1, sizeof(IRBuiltinRegistry));

    registry->capacity = 128;
    registry->builtins = (IRBuiltinDef*)calloc(registry->capacity, sizeof(IRBuiltinDef));
    registry->bucket_count = 32;
    registry->buckets = (IRBuiltinDef**)calloc(registry->bucket_count, sizeof(IRBuiltinDef*));

    // Register all builtin functions
    register_string_functions(registry);
    register_array_functions(registry);
    register_math_functions(registry);
    register_type_functions(registry);
    register_utility_functions(registry);

    return registry;
}

// Register a builtin function
void ir_builtin_register(IRBuiltinRegistry* registry, const char* name,
                        IRBuiltinFunc func, const char* signature,
                        const char* description, uint32_t min_args, uint32_t max_args,
                        VarType return_type, bool is_pure) {

    // Grow array if needed
    if (registry->count >= registry->capacity) {
        registry->capacity *= 2;
        registry->builtins = (IRBuiltinDef*)realloc(registry->builtins,
            registry->capacity * sizeof(IRBuiltinDef));
    }

    IRBuiltinDef* def = &registry->builtins[registry->count++];

    def->name = strdup(name);
    def->func = func;
    def->signature = strdup(signature);
    def->description = strdup(description);
    def->min_args = min_args;
    def->max_args = max_args;
    def->return_type = return_type;
    def->is_pure = is_pure;
    def->arg_types = NULL;
    def->arg_type_count = 0;

    // Add to hash table
    uint64_t hash = hash_string(name);
    uint32_t bucket = hash & (registry->bucket_count - 1);

    // Insert at head of bucket chain
    def->next_in_bucket = registry->buckets[bucket];
    registry->buckets[bucket] = def;
}

// Lookup builtin by name
IRBuiltinDef* ir_builtin_lookup(IRBuiltinRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;

    uint64_t hash = hash_string(name);
    uint32_t bucket = hash & (registry->bucket_count - 1);

    IRBuiltinDef* def = registry->buckets[bucket];
    while (def) {
        if (strcmp(def->name, name) == 0) {
            return def;
        }
        def = def->next_in_bucket;
    }

    return NULL;
}

// Call builtin function
IRValue ir_builtin_call(IRBuiltinRegistry* registry, const char* name,
                        IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    IRBuiltinDef* def = ir_builtin_lookup(registry, name);
    if (!def) {
        // Error: unknown function
        return ir_value_null();
    }

    // Validate argument count
    if (arg_count < def->min_args ||
        (def->max_args > 0 && arg_count > def->max_args)) {
        // Error: wrong argument count
        return ir_value_null();
    }

    // Call function
    return def->func(ctx, args, arg_count);
}
```

---

## String Functions

### 2.1 String Manipulation

```c
// String.toUpperCase(str: string) -> string
static IRValue builtin_string_toUpper(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val;
    char* upper = strdup(str);

    for (size_t i = 0; upper[i]; i++) {
        upper[i] = toupper((unsigned char)upper[i]);
    }

    return ir_value_string_nocopy(upper);
}

// String.toLowerCase(str: string) -> string
static IRValue builtin_string_toLower(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val;
    char* lower = strdup(str);

    for (size_t i = 0; lower[i]; i++) {
        lower[i] = tolower((unsigned char)lower[i]);
    }

    return ir_value_string_nocopy(lower);
}

// String.trim(str: string) -> string
static IRValue builtin_string_trim(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val;
    if (!str) return ir_value_string("");

    // Find start
    const char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;

    // Find end
    const char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    // Copy trimmed string
    size_t len = end - start + 1;
    char* trimmed = (char*)malloc(len + 1);
    memcpy(trimmed, start, len);
    trimmed[len] = '\0';

    return ir_value_string_nocopy(trimmed);
}

// String.substring(str: string, start: int, end?: int) -> string
static IRValue builtin_string_substring(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_STRING ||
        args[1].type != VAR_TYPE_INT) {
        return ir_value_null();
    }

    const char* str = args[0].string_val;
    if (!str) return ir_value_string("");

    int64_t start = args[1].int_val;
    int64_t len = strlen(str);

    // Default end to string length
    int64_t end = (arg_count >= 3 && args[2].type == VAR_TYPE_INT) ?
                  args[2].int_val : len;

    // Clamp values
    if (start < 0) start = 0;
    if (start > len) start = len;
    if (end < 0) end = 0;
    if (end > len) end = len;
    if (end < start) end = start;

    // Extract substring
    size_t sub_len = end - start;
    char* substr = (char*)malloc(sub_len + 1);
    memcpy(substr, str + start, sub_len);
    substr[sub_len] = '\0';

    return ir_value_string_nocopy(substr);
}

// String.split(str: string, delimiter: string) -> array
static IRValue builtin_string_split(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_STRING ||
        args[1].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val;
    const char* delim = args[1].string_val;

    if (!str) str = "";
    if (!delim) delim = "";

    IRValue result = ir_value_array_empty();

    if (strlen(delim) == 0) {
        // Split by individual characters
        for (size_t i = 0; str[i]; i++) {
            char ch[2] = {str[i], '\0'};
            IRValue elem = ir_value_string(ch);
            // Add to array
            if (result.array_val.count >= result.array_val.capacity) {
                result.array_val.capacity *= 2;
                result.array_val.items = (IRValue*)realloc(result.array_val.items,
                    result.array_val.capacity * sizeof(IRValue));
            }
            result.array_val.items[result.array_val.count++] = elem;
        }
    } else {
        // Split by delimiter
        char* str_copy = strdup(str);
        char* token = strtok(str_copy, delim);

        while (token) {
            IRValue elem = ir_value_string(token);
            if (result.array_val.count >= result.array_val.capacity) {
                result.array_val.capacity *= 2;
                result.array_val.items = (IRValue*)realloc(result.array_val.items,
                    result.array_val.capacity * sizeof(IRValue));
            }
            result.array_val.items[result.array_val.count++] = elem;
            token = strtok(NULL, delim);
        }

        free(str_copy);
    }

    return result;
}

// String.length(str: string) -> int
static IRValue builtin_string_length(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_int(0);
    }

    const char* str = args[0].string_val;
    return ir_value_int(str ? strlen(str) : 0);
}

// String.indexOf(str: string, search: string, start?: int) -> int
static IRValue builtin_string_indexOf(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_STRING ||
        args[1].type != VAR_TYPE_STRING) {
        return ir_value_int(-1);
    }

    const char* str = args[0].string_val;
    const char* search = args[1].string_val;

    if (!str) str = "";
    if (!search) search = "";

    int64_t start = 0;
    if (arg_count >= 3 && args[2].type == VAR_TYPE_INT) {
        start = args[2].int_val;
        if (start < 0) start = 0;
    }

    const char* found = strstr(str + start, search);
    if (found) {
        return ir_value_int(found - str);
    }

    return ir_value_int(-1);
}

// Register all string functions
static void register_string_functions(IRBuiltinRegistry* registry) {
    // Instance methods (called on string objects)
    // These are registered differently - they're called via method dispatch

    // Global/utility functions
    ir_builtin_register(registry, "string_toUpper", builtin_string_toUpper,
        "(str: string) -> string", "Convert string to uppercase", 1, 1, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "string_toLower", builtin_string_toLower,
        "(str: string) -> string", "Convert string to lowercase", 1, 1, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "string_trim", builtin_string_trim,
        "(str: string) -> string", "Remove whitespace from both ends", 1, 1, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "string_substring", builtin_string_substring,
        "(str: string, start: int, end?: int) -> string", "Extract substring", 2, 3, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "string_split", builtin_string_split,
        "(str: string, delimiter: string) -> array", "Split string by delimiter", 2, 2, VAR_TYPE_ARRAY, true);
}
```

---

## Array Functions

### 3.1 Array Manipulation

```c
// Array.length(arr: array) -> int
static IRValue builtin_array_length(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_int(0);
    }

    return ir_value_int(args[0].array_val.count);
}

// Array.push(arr: array, value: any) -> array
static IRValue builtin_array_push(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    IRValue* array = &args[0];

    // Resize if needed
    if (array->array_val.count >= array->array_val.capacity) {
        array->array_val.capacity *= 2;
        array->array_val.items = (IRValue*)realloc(array->array_val.items,
            array->array_val.capacity * sizeof(IRValue));
    }

    // Add element
    array->array_val.items[array->array_val.count++] = ir_value_copy(&args[1]);

    // Return array for chaining
    return ir_value_copy(array);
}

// Array.pop(arr: array) -> any
static IRValue builtin_array_pop(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    IRValue* array = &args[0];

    if (array->array_val.count == 0) {
        return ir_value_null();
    }

    IRValue result = array->array_val.items[--array->array_val.count];
    return result;
}

// Array.indexOf(arr: array, value: any) -> int
static IRValue builtin_array_indexOf(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_int(-1);
    }

    IRValue* array = &args[0];
    IRValue* search = &args[1];

    for (uint32_t i = 0; i < array->array_val.count; i++) {
        if (values_equal(&array->array_val.items[i], search)) {
            return ir_value_int(i);
        }
    }

    return ir_value_int(-1);
}

// Array.join(arr: array, separator?: string) -> string
static IRValue builtin_array_join(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_string("");
    }

    IRValue* array = &args[0];
    const char* separator = ",";

    if (arg_count >= 2 && args[1].type == VAR_TYPE_STRING) {
        separator = args[1].string_val;
    }

    // Calculate total length
    size_t total_len = 0;
    for (uint32_t i = 0; i < array->array_val.count; i++) {
        if (i > 0) total_len += strlen(separator);
        if (array->array_val.items[i].type == VAR_TYPE_STRING) {
            total_len += strlen(array->array_val.items[i].string_val);
        } else {
            char buf[64];
            value_to_string(array->array_val.items[i], buf, sizeof(buf));
            total_len += strlen(buf);
        }
    }

    // Build result
    char* result = (char*)malloc(total_len + 1);
    char* ptr = result;

    for (uint32_t i = 0; i < array->array_val.count; i++) {
        if (i > 0) {
            strcpy(ptr, separator);
            ptr += strlen(separator);
        }

        if (array->array_val.items[i].type == VAR_TYPE_STRING) {
            strcpy(ptr, array->array_val.items[i].string_val);
            ptr += strlen(array->array_val.items[i].string_val);
        } else {
            char buf[64];
            value_to_string(array->array_val.items[i], buf, sizeof(buf));
            strcpy(ptr, buf);
            ptr += strlen(buf);
        }
    }

    *ptr = '\0';
    return ir_value_string_nocopy(result);
}

// Array.slice(arr: array, start: int, end?: int) -> array
static IRValue builtin_array_slice(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY ||
        args[1].type != VAR_TYPE_INT) {
        return ir_value_null();
    }

    IRValue* array = &args[0];
    int64_t start = args[1].int_val;
    int64_t len = array->array_val.count;

    // Default end to length
    int64_t end = (arg_count >= 3 && args[2].type == VAR_TYPE_INT) ?
                  args[2].int_val : len;

    // Handle negative indices
    if (start < 0) start = len + start;
    if (start < 0) start = 0;
    if (end < 0) end = len + end;
    if (end < 0) end = 0;
    if (start > len) start = len;
    if (end > len) end = len;
    if (end < start) end = start;

    // Create result array
    IRValue result = ir_value_array_empty();
    for (int64_t i = start; i < end; i++) {
        if (result.array_val.count >= result.array_val.capacity) {
            result.array_val.capacity *= 2;
            result.array_val.items = (IRValue*)realloc(result.array_val.items,
                result.array_val.capacity * sizeof(IRValue));
        }
        result.array_val.items[result.array_val.count++] =
            ir_value_copy(&array->array_val.items[i]);
    }

    return result;
}

// Register array functions
static void register_array_functions(IRBuiltinRegistry* registry) {
    ir_builtin_register(registry, "array_length", builtin_array_length,
        "(arr: array) -> int", "Get array length", 1, 1, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "array_push", builtin_array_push,
        "(arr: array, value: any) -> array", "Add element to end", 2, 2, VAR_TYPE_ARRAY, false);
    ir_builtin_register(registry, "array_pop", builtin_array_pop,
        "(arr: array) -> any", "Remove and return last element", 1, 1, VAR_TYPE_NULL, false);
    ir_builtin_register(registry, "array_indexOf", builtin_array_indexOf,
        "(arr: array, value: any) -> int", "Find element index", 2, 2, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "array_join", builtin_array_join,
        "(arr: array, separator?: string) -> string", "Join elements as string", 1, 2, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "array_slice", builtin_array_slice,
        "(arr: array, start: int, end?: int) -> array", "Extract portion of array", 2, 3, VAR_TYPE_ARRAY, true);
}
```

---

## Math Functions

### 4.1 Math Operations

```c
// Math.abs(x: int) -> int
static IRValue builtin_math_abs(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1 || args[0].type != VAR_TYPE_INT) {
        return ir_value_int(0);
    }

    int64_t val = args[0].int_val;
    return ir_value_int(val < 0 ? -val : val);
}

// Math.min(a: int, b: int) -> int
static IRValue builtin_math_min(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1) return ir_value_int(0);

    int64_t result = (args[0].type == VAR_TYPE_INT) ? args[0].int_val : 0;

    for (uint32_t i = 1; i < arg_count; i++) {
        if (args[i].type == VAR_TYPE_INT && args[i].int_val < result) {
            result = args[i].int_val;
        }
    }

    return ir_value_int(result);
}

// Math.max(a: int, b: int) -> int
static IRValue builtin_math_max(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1) return ir_value_int(0);

    int64_t result = (args[0].type == VAR_TYPE_INT) ? args[0].int_val : 0;

    for (uint32_t i = 1; i < arg_count; i++) {
        if (args[i].type == VAR_TYPE_INT && args[i].int_val > result) {
            result = args[i].int_val;
        }
    }

    return ir_value_int(result);
}

// Math.clamp(value: int, min: int, max: int) -> int
static IRValue builtin_math_clamp(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 3) return ir_value_int(0);

    int64_t value = (args[0].type == VAR_TYPE_INT) ? args[0].int_val : 0;
    int64_t min = (args[1].type == VAR_TYPE_INT) ? args[1].int_val : 0;
    int64_t max = (args[2].type == VAR_TYPE_INT) ? args[2].int_val : 0;

    if (value < min) value = min;
    if (value > max) value = max;

    return ir_value_int(value);
}

// Math.random() -> int (0 to RAND_MAX)
static IRValue builtin_math_random(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    return ir_value_int(rand());
}

// Math.floor(value: int) -> int
static IRValue builtin_math_floor(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    // For integers, floor is identity
    if (arg_count < 1 || args[0].type != VAR_TYPE_INT) {
        return ir_value_int(0);
    }
    return ir_value_int(args[0].int_val);
}

// Math.ceil(value: int) -> int
static IRValue builtin_math_ceil(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    // For integers, ceil is identity
    if (arg_count < 1 || args[0].type != VAR_TYPE_INT) {
        return ir_value_int(0);
    }
    return ir_value_int(args[0].int_val);
}

// Register math functions
static void register_math_functions(IRBuiltinRegistry* registry) {
    ir_builtin_register(registry, "math_abs", builtin_math_abs,
        "(x: int) -> int", "Absolute value", 1, 1, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_min", builtin_math_min,
        "(...values: int[]) -> int", "Minimum value", 1, 0, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_max", builtin_math_max,
        "(...values: int[]) -> int", "Maximum value", 1, 0, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_clamp", builtin_math_clamp,
        "(value: int, min: int, max: int) -> int", "Clamp value to range", 3, 3, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_random", builtin_math_random,
        "() -> int", "Random number", 0, 0, VAR_TYPE_INT, false);
}
```

---

## Type Functions

### 5.1 Type Conversion

```c
// String.toInt(str: string) -> int
static IRValue builtin_type_toInt(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1) return ir_value_int(0);

    if (args[0].type == VAR_TYPE_INT) {
        return ir_value_int(args[0].int_val);
    }

    if (args[0].type == VAR_TYPE_STRING) {
        return ir_value_int(atoll(args[0].string_val));
    }

    if (args[0].type == VAR_TYPE_BOOL) {
        return ir_value_int(args[0].bool_val ? 1 : 0);
    }

    return ir_value_int(0);
}

// String.toString(value: any) -> string
static IRValue builtin_type_toString(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1) return ir_value_string("");

    char buffer[256];
    value_to_string(args[0], buffer, sizeof(buffer));
    return ir_value_string(buffer);
}

// String.typeof(value: any) -> string
static IRValue builtin_type_typeof(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    if (arg_count < 1) return ir_value_string("null");

    const char* type_str = "null";
    switch (args[0].type) {
        case VAR_TYPE_INT:    type_str = "int"; break;
        case VAR_TYPE_STRING: type_str = "string"; break;
        case VAR_TYPE_BOOL:   type_str = "bool"; break;
        case VAR_TYPE_ARRAY:  type_str = "array"; break;
        case VAR_TYPE_OBJECT: type_str = "object"; break;
        case VAR_TYPE_NULL:   type_str = "null"; break;
    }

    return ir_value_string(type_str);
}

// Register type functions
static void register_type_functions(IRBuiltinRegistry* registry) {
    ir_builtin_register(registry, "type_toInt", builtin_type_toInt,
        "(value: any) -> int", "Convert to integer", 1, 1, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "type_toString", builtin_type_toString,
        "(value: any) -> string", "Convert to string", 1, 1, VAR_TYPE_STRING, true);
    ir_builtin_register(registry, "type_typeof", builtin_type_typeof,
        "(value: any) -> string", "Get type name", 1, 1, VAR_TYPE_STRING, true);
}
```

---

## Implementation Steps

### Day 1: Registry & String Functions

1. **Morning** (4 hours)
   - [ ] Create `ir/ir_builtin_registry.h` and `.c`
   - [ ] Implement registry structure
   - [ ] Implement registration and lookup
   - [ ] Write unit tests

2. **Afternoon** (4 hours)
   - [ ] Implement string functions
   - [ ] Test string functions
   - [ ] Add documentation

### Day 2: Array Functions

1. **Morning** (4 hours)
   - [ ] Implement array functions
   - [ ] Test array functions
   - [ ] Add error handling

2. **Afternoon** (4 hours)
   - [ ] Implement array iteration helpers
   - [ ] Add array functional methods (map, filter placeholder)
   - [ ] Documentation

### Day 3: Math & Type Functions

1. **Morning** (4 hours)
   - [ ] Implement math functions
   - [ ] Test math functions
   - [ ] Add random seed initialization

2. **Afternoon** (4 hours)
   - [ ] Implement type conversion functions
   - [ ] Test type functions
   - [ ] Documentation

### Day 4: Integration & Testing

1. **Morning** (4 hours)
   - [ ] Integrate with evaluator
   - [ ] Add method dispatch for string/array methods
   - [ ] Test from TSX handlers

2. **Afternoon** (4 hours)
   - [ ] Comprehensive test suite
   - [ ] Performance benchmarks
   - [ ] Final documentation

---

## Testing Strategy

```c
// Test string functions
void test_string_functions(void) {
    // toUpperCase
    IRValue str = ir_value_string("hello");
    IRValue result = builtin_string_toUpper(NULL, &str, 1);
    assert(strcmp(result.string_val, "HELLO") == 0);

    // split
    IRValue delim = ir_value_string(",");
    str = ir_value_string("a,b,c");
    IRValue args[2] = {str, delim};
    result = builtin_string_split(NULL, args, 2);
    assert(result.type == VAR_TYPE_ARRAY);
    assert(result.array_val.count == 3);
}

// Test array functions
void test_array_functions(void) {
    IRValue arr = ir_value_array_empty();
    IRValue elem = ir_value_int(42);

    IRValue args[2] = {arr, elem};
    IRValue result = builtin_array_push(NULL, args, 2);

    assert(result.array_val.count == 1);
    assert(result.array_val.items[0].int_val == 42);
}
```

---

## Acceptance Criteria

Phase 6 is complete when:

- [ ] All string functions work correctly
- [ ] All array functions work correctly
- [ ] All math functions work correctly
- [ ] All type functions work correctly
- [ ] Functions are callable from expressions
- [ ] Method dispatch works (e.g., `"hello".toUpperCase()`)
- [ ] Unit test coverage > 85%
- [ ] Documentation is complete

---

## Next Phase

Once Phase 6 is complete, proceed to **[Phase 7: Integration & Testing](./phase-7-integration-testing.md)**.

---

**Last Updated**: 2026-01-11
