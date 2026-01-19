#define _POSIX_C_SOURCE 200809L
#include "ir_builtin_registry.h"
#include "../include/ir_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

// ============================================================================
// HASH FUNCTIONS
// ============================================================================

static uint64_t hash_string(const char* str) {
    if (!str) return 0;

    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint64_t)(unsigned char)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

__attribute__((unused)) static uint32_t next_power_of_2(uint32_t value) {
    if (value == 0) return 1;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return value;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Helper: Create string value that takes ownership of allocated string
static IRValue ir_value_string_nocopy(char* str) {
    IRValue v;
    v.type = VAR_TYPE_STRING;
    v.string_val = str;
    return v;
}

// Convert value to string (returns newly allocated string)
char* ir_value_to_string_alloc(const IRValue* value) {
    if (!value) return strdup("null");

    char buffer[256];
    const char* result = buffer;

    switch (value->type) {
        case VAR_TYPE_NULL:
            result = "null";
            break;
        case VAR_TYPE_INT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int_val);
            break;
        case VAR_TYPE_FLOAT:
            snprintf(buffer, sizeof(buffer), "%g", value->float_val);
            break;
        case VAR_TYPE_STRING:
            return strdup(value->string_val ? value->string_val : "");
        case VAR_TYPE_BOOL:
            result = value->bool_val ? "true" : "false";
            break;
        case VAR_TYPE_ARRAY: {
            snprintf(buffer, sizeof(buffer), "<array[%u]>", value->array_val ? value->array_val->count : 0);
            break;
        }
        case VAR_TYPE_OBJECT:
            result = "<object>";
            break;
        default:
            result = "<unknown>";
            break;
    }

    return strdup(result);
}

// Compare two values for equality
bool ir_value_equals_builtin(const IRValue* a, const IRValue* b) {
    if (!a || !b) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case VAR_TYPE_NULL:
            return true;
        case VAR_TYPE_INT:
            return a->int_val == b->int_val;
        case VAR_TYPE_FLOAT:
            return a->float_val == b->float_val;
        case VAR_TYPE_BOOL:
            return a->bool_val == b->bool_val;
        case VAR_TYPE_STRING:
            return strcmp(a->string_val ? a->string_val : "",
                        b->string_val ? b->string_val : "") == 0;
        default:
            return false;
    }
}

// ============================================================================
// STRING FUNCTIONS
// ============================================================================

// string_toUpper(str: string) -> string
static IRValue builtin_string_toUpper(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val ? args[0].string_val : "";
    char* upper = strdup(str);

    for (size_t i = 0; upper[i]; i++) {
        upper[i] = toupper((unsigned char)upper[i]);
    }

    return ir_value_string_nocopy(upper);
}

// string_toLower(str: string) -> string
static IRValue builtin_string_toLower(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val ? args[0].string_val : "";
    char* lower = strdup(str);

    for (size_t i = 0; lower[i]; i++) {
        lower[i] = tolower((unsigned char)lower[i]);
    }

    return ir_value_string_nocopy(lower);
}

// string_trim(str: string) -> string
static IRValue builtin_string_trim(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val ? args[0].string_val : "";
    if (!str[0]) return ir_value_string("");

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

// string_substring(str: string, start: int, end?: int) -> string
static IRValue builtin_string_substring(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAR_TYPE_STRING || args[1].type != VAR_TYPE_INT) {
        return ir_value_null();
    }

    const char* str = args[0].string_val ? args[0].string_val : "";
    int64_t len = (int64_t)strlen(str);
    int64_t start = args[1].int_val;

    // Default end to string length
    int64_t end = (arg_count >= 3 && args[2].type == VAR_TYPE_INT) ? args[2].int_val : len;

    // Clamp values
    if (start < 0) start = 0;
    if (start > len) start = len;
    if (end < 0) end = 0;
    if (end > len) end = len;
    if (end < start) end = start;

    // Extract substring
    size_t sub_len = (size_t)(end - start);
    char* substr = (char*)malloc(sub_len + 1);
    memcpy(substr, str + start, sub_len);
    substr[sub_len] = '\0';

    return ir_value_string_nocopy(substr);
}

// string_split(str: string, delimiter: string) -> array
static IRValue builtin_string_split(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAR_TYPE_STRING || args[1].type != VAR_TYPE_STRING) {
        return ir_value_null();
    }

    const char* str = args[0].string_val ? args[0].string_val : "";
    const char* delim = args[1].string_val ? args[1].string_val : "";

    IRValue result = ir_value_array_empty();

    if (delim[0] == '\0') {
        // Split by individual characters
        for (size_t i = 0; str[i]; i++) {
            char ch[2] = {str[i], '\0'};
            IRValue elem = ir_value_string(ch);
            ir_array_push(result.array_val, elem);
            // Note: ir_array_push takes ownership, so don't free elem
        }
    } else {
        // Split by delimiter
        char* str_copy = strdup(str);
        char* token = strtok(str_copy, delim);

        while (token) {
            IRValue elem = ir_value_string(token);
            ir_array_push(result.array_val, elem);
            // Note: ir_array_push takes ownership, so don't free elem
            token = strtok(NULL, delim);
        }

        free(str_copy);
    }

    return result;
}

// string_length(str: string) -> int
static IRValue builtin_string_length(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_STRING) {
        return ir_value_int(0);
    }

    const char* str = args[0].string_val;
    return ir_value_int(str ? (int64_t)strlen(str) : 0);
}

// Register all string functions
static void register_string_functions(IRBuiltinRegistry* registry) {
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
    ir_builtin_register(registry, "string_length", builtin_string_length,
        "(str: string) -> int", "Get string length", 1, 1, VAR_TYPE_INT, true);
}

// ============================================================================
// ARRAY FUNCTIONS
// ============================================================================

// array_length(arr: array) -> int
static IRValue builtin_array_length(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_int(0);
    }

    return ir_value_int(ir_array_length(args[0].array_val));
}

// array_push(arr: array, value: any) -> array
static IRValue builtin_array_push(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    IRArray* array = args[0].array_val;
    ir_array_push(array, ir_value_copy(&args[1]));

    // Return array for chaining
    return ir_value_copy(&args[0]);
}

// array_pop(arr: array) -> any
static IRValue builtin_array_pop(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    return ir_array_pop(args[0].array_val);
}

// array_indexOf(arr: array, value: any) -> int
static IRValue builtin_array_indexOf(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_int(-1);
    }

    IRArray* array = args[0].array_val;
    IRValue* search = &args[1];

    for (uint32_t i = 0; i < ir_array_length(array); i++) {
        IRValue elem = ir_array_get(array, i);
        bool equal = ir_value_equals_builtin(&elem, search);
        ir_value_free(&elem);
        if (equal) return ir_value_int(i);
    }

    return ir_value_int(-1);
}

// array_join(arr: array, separator?: string) -> string
static IRValue builtin_array_join(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_string("");
    }

    IRArray* array = args[0].array_val;
    const char* separator = ",";

    if (arg_count >= 2 && args[1].type == VAR_TYPE_STRING && args[1].string_val) {
        separator = args[1].string_val;
    }

    // Calculate total length
    size_t total_len = 0;
    uint32_t count = ir_array_length(array);
    for (uint32_t i = 0; i < count; i++) {
        if (i > 0) total_len += strlen(separator);
        IRValue elem = ir_array_get(array, i);
        if (elem.type == VAR_TYPE_STRING && elem.string_val) {
            total_len += strlen(elem.string_val);
        } else {
            char* elem_str = ir_value_to_string_alloc(&elem);
            total_len += strlen(elem_str);
            free(elem_str);
        }
        ir_value_free(&elem);
    }

    // Build result
    char* result = (char*)malloc(total_len + 1);
    char* ptr = result;

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0) {
            strcpy(ptr, separator);
            ptr += strlen(separator);
        }

        IRValue elem = ir_array_get(array, i);
        if (elem.type == VAR_TYPE_STRING && elem.string_val) {
            strcpy(ptr, elem.string_val);
            ptr += strlen(elem.string_val);
        } else {
            char* elem_str = ir_value_to_string_alloc(&elem);
            strcpy(ptr, elem_str);
            ptr += strlen(elem_str);
            free(elem_str);
        }
        ir_value_free(&elem);
    }

    *ptr = '\0';
    return ir_value_string_nocopy(result);
}

// array_slice(arr: array, start: int, end?: int) -> array
static IRValue builtin_array_slice(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAR_TYPE_ARRAY || args[1].type != VAR_TYPE_INT) {
        return ir_value_null();
    }

    IRArray* array = args[0].array_val;
    int64_t len = ir_array_length(array);
    int64_t start = args[1].int_val;

    // Default end to length
    int64_t end = (arg_count >= 3 && args[2].type == VAR_TYPE_INT) ? args[2].int_val : len;

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
        IRValue elem = ir_array_get(array, (uint32_t)i);
        ir_array_push(result.array_val, elem);
    }

    return result;
}

// array_reverse(arr: array) -> array
static IRValue builtin_array_reverse(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    IRArray* array = args[0].array_val;
    uint32_t len = ir_array_length(array);

    // In-place reverse
    for (uint32_t i = 0; i < len / 2; i++) {
        IRValue a = ir_array_get(array, i);
        IRValue b = ir_array_get(array, len - 1 - i);

        // Swap
        array->items[len - 1 - i] = a;
        array->items[i] = b;
    }

    return ir_value_copy(&args[0]);
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
    ir_builtin_register(registry, "array_reverse", builtin_array_reverse,
        "(arr: array) -> array", "Reverse array in-place", 1, 1, VAR_TYPE_ARRAY, false);
}

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

// math_abs(x: int) -> int
static IRValue builtin_math_abs(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_int(0);

    int64_t val = 0;
    if (args[0].type == VAR_TYPE_INT) {
        val = args[0].int_val;
    } else if (args[0].type == VAR_TYPE_FLOAT) {
        double fval = args[0].float_val;
        return ir_value_int((int64_t)fabs(fval));
    }

    return ir_value_int(val < 0 ? -val : val);
}

// math_min(...values: int[]) -> int
static IRValue builtin_math_min(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_int(0);

    int64_t result = 0;
    if (args[0].type == VAR_TYPE_INT) {
        result = args[0].int_val;
    } else if (args[0].type == VAR_TYPE_FLOAT) {
        double fresult = args[0].float_val;
        for (uint32_t i = 1; i < arg_count; i++) {
            if (args[i].type == VAR_TYPE_FLOAT && args[i].float_val < fresult) {
                fresult = args[i].float_val;
            }
        }
        return ir_value_float(fresult);
    }

    for (uint32_t i = 1; i < arg_count; i++) {
        if (args[i].type == VAR_TYPE_INT && args[i].int_val < result) {
            result = args[i].int_val;
        }
    }

    return ir_value_int(result);
}

// math_max(...values: int[]) -> int
static IRValue builtin_math_max(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_int(0);

    int64_t result = 0;
    if (args[0].type == VAR_TYPE_INT) {
        result = args[0].int_val;
    } else if (args[0].type == VAR_TYPE_FLOAT) {
        double fresult = args[0].float_val;
        for (uint32_t i = 1; i < arg_count; i++) {
            if (args[i].type == VAR_TYPE_FLOAT && args[i].float_val > fresult) {
                fresult = args[i].float_val;
            }
        }
        return ir_value_float(fresult);
    }

    for (uint32_t i = 1; i < arg_count; i++) {
        if (args[i].type == VAR_TYPE_INT && args[i].int_val > result) {
            result = args[i].int_val;
        }
    }

    return ir_value_int(result);
}

// math_clamp(value: int, min: int, max: int) -> int
static IRValue builtin_math_clamp(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 3) return ir_value_int(0);

    int64_t value = 0, min = 0, max = 0;

    if (args[0].type == VAR_TYPE_INT) value = args[0].int_val;
    if (args[1].type == VAR_TYPE_INT) min = args[1].int_val;
    if (args[2].type == VAR_TYPE_INT) max = args[2].int_val;

    if (value < min) value = min;
    if (value > max) value = max;

    return ir_value_int(value);
}

// math_random() -> int
static IRValue builtin_math_random(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    (void)args;
    (void)arg_count;
    return ir_value_int(rand());
}

// Register math functions
static void register_math_functions(IRBuiltinRegistry* registry) {
    ir_builtin_register(registry, "math_abs", builtin_math_abs,
        "(x: number) -> number", "Absolute value", 1, 1, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_min", builtin_math_min,
        "(...values: number[]) -> number", "Minimum value", 1, 0, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_max", builtin_math_max,
        "(...values: number[]) -> number", "Maximum value", 1, 0, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_clamp", builtin_math_clamp,
        "(value: number, min: number, max: number) -> number", "Clamp value to range", 3, 3, VAR_TYPE_INT, true);
    ir_builtin_register(registry, "math_random", builtin_math_random,
        "() -> int", "Random number", 0, 0, VAR_TYPE_INT, false);
}

// ============================================================================
// TYPE FUNCTIONS
// ============================================================================

// type_toInt(value: any) -> int
static IRValue builtin_type_toInt(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_int(0);

    switch (args[0].type) {
        case VAR_TYPE_INT:
            return ir_value_int(args[0].int_val);
        case VAR_TYPE_FLOAT:
            return ir_value_int((int64_t)args[0].float_val);
        case VAR_TYPE_STRING:
            return ir_value_int(atoll(args[0].string_val ? args[0].string_val : "0"));
        case VAR_TYPE_BOOL:
            return ir_value_int(args[0].bool_val ? 1 : 0);
        default:
            return ir_value_int(0);
    }
}

// type_toString(value: any) -> string
static IRValue builtin_type_toString(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_string("");

    char* str = ir_value_to_string_alloc(&args[0]);
    return ir_value_string_nocopy(str);
}

// type_typeof(value: any) -> string
static IRValue builtin_type_typeof(IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    (void)ctx;
    if (arg_count < 1) return ir_value_string("null");

    const char* type_str = "null";
    switch (args[0].type) {
        case VAR_TYPE_INT:      type_str = "int"; break;
        case VAR_TYPE_FLOAT:    type_str = "float"; break;
        case VAR_TYPE_STRING:   type_str = "string"; break;
        case VAR_TYPE_BOOL:     type_str = "bool"; break;
        case VAR_TYPE_ARRAY:    type_str = "array"; break;
        case VAR_TYPE_OBJECT:   type_str = "object"; break;
        case VAR_TYPE_NULL:     type_str = "null"; break;
        case VAR_TYPE_FUNCTION: type_str = "function"; break;
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

// ============================================================================
// REGISTRY IMPLEMENTATION
// ============================================================================

// Create builtin registry with all standard functions
IRBuiltinRegistry* ir_builtin_registry_create(void) {
    IRBuiltinRegistry* registry = (IRBuiltinRegistry*)calloc(1, sizeof(IRBuiltinRegistry));
    if (!registry) return NULL;

    registry->capacity = 128;
    registry->builtins = (IRBuiltinDef*)calloc(registry->capacity, sizeof(IRBuiltinDef));
    if (!registry->builtins) {
        free(registry);
        return NULL;
    }

    registry->bucket_count = 32;
    registry->buckets = (IRBuiltinDef**)calloc(registry->bucket_count, sizeof(IRBuiltinDef*));
    if (!registry->buckets) {
        free(registry->builtins);
        free(registry);
        return NULL;
    }

    // Register all builtin functions
    register_string_functions(registry);
    register_array_functions(registry);
    register_math_functions(registry);
    register_type_functions(registry);

    IR_LOG_DEBUG("BUILTIN", "Registered %u builtin functions", registry->count);

    return registry;
}

// Destroy registry and free all builtins
void ir_builtin_registry_destroy(IRBuiltinRegistry* registry) {
    if (!registry) return;

    // Free builtin definitions
    for (uint32_t i = 0; i < registry->count; i++) {
        free((void*)registry->builtins[i].name);
        free((void*)registry->builtins[i].signature);
        free((void*)registry->builtins[i].description);
    }

    free(registry->builtins);
    free(registry->buckets);
    free(registry);
}

// Register a builtin function
void ir_builtin_register(IRBuiltinRegistry* registry, const char* name,
                        IRBuiltinFunc func, const char* signature,
                        const char* description, uint32_t min_args, uint32_t max_args,
                        VarType return_type, bool is_pure) {
    if (!registry || !name) return;

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
    def->next_in_bucket = NULL;

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

// Call builtin function by name
IRValue ir_builtin_call(IRBuiltinRegistry* registry, const char* name,
                       IREvalContext* ctx, IRValue* args, uint32_t arg_count) {
    IRBuiltinDef* def = ir_builtin_lookup(registry, name);
    if (!def) {
        IR_LOG_WARN("BUILTIN", "Unknown builtin function: %s", name);
        return ir_value_null();
    }

    // Validate argument count
    if (arg_count < def->min_args ||
        (def->max_args > 0 && arg_count > def->max_args)) {
        IR_LOG_WARN("BUILTIN", "Argument count mismatch for %s: expected %u-%u, got %u",
                   name, def->min_args, def->max_args, arg_count);
        return ir_value_null();
    }

    // Call function
    return def->func(ctx, args, arg_count);
}

// ============================================================================
// GLOBAL REGISTRY
// ============================================================================

static IRBuiltinRegistry* g_builtin_registry = NULL;

// Initialize global builtin registry
void ir_builtin_global_init(void) {
    if (g_builtin_registry) {
        ir_builtin_registry_destroy(g_builtin_registry);
    }

    g_builtin_registry = ir_builtin_registry_create();
}

// Get global builtin registry
IRBuiltinRegistry* ir_builtin_global_get(void) {
    return g_builtin_registry;
}

// Cleanup global builtin registry
void ir_builtin_global_cleanup(void) {
    if (g_builtin_registry) {
        ir_builtin_registry_destroy(g_builtin_registry);
        g_builtin_registry = NULL;
    }
}

// Check if global registry is initialized
bool ir_builtin_global_is_initialized(void) {
    return g_builtin_registry != NULL;
}
