/**
 * KRY Arrow Function Registry Implementation
 *
 * Manages collection and C code generation for arrow functions.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_arrow_registry.h"
#include "kry_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Buffer Utilities (same pattern as kry_expression.c)
// ============================================================================

typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} ArrowBuffer;

static void abuf_init(ArrowBuffer* buf) {
    buf->capacity = 256;
    buf->size = 0;
    buf->buffer = (char*)malloc(buf->capacity);
    buf->buffer[0] = '\0';
}

static void abuf_append(ArrowBuffer* buf, const char* str) {
    size_t len = strlen(str);
    if (buf->size + len >= buf->capacity) {
        while (buf->size + len >= buf->capacity) {
            buf->capacity *= 2;
        }
        buf->buffer = (char*)realloc(buf->buffer, buf->capacity);
    }
    strcpy(buf->buffer + buf->size, str);
    buf->size += len;
}

static void abuf_append_int(ArrowBuffer* buf, uint32_t n) {
    char num[32];
    snprintf(num, sizeof(num), "%u", n);
    abuf_append(buf, num);
}

static char* abuf_extract(ArrowBuffer* buf) {
    return buf->buffer;
}

static void abuf_free(ArrowBuffer* buf) {
    free(buf->buffer);
    buf->buffer = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

// ============================================================================
// String Array Utilities
// ============================================================================

static char** copy_string_array(const char** src, size_t count) {
    if (count == 0 || !src) return NULL;

    char** dst = (char**)malloc(count * sizeof(char*));
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i] ? strdup(src[i]) : NULL;
    }
    return dst;
}

static void free_string_array(char** arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

static bool string_in_array(const char* str, const char** arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (arr[i] && strcmp(str, arr[i]) == 0) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Registry Implementation
// ============================================================================

KryArrowRegistry* kry_arrow_registry_create(void) {
    KryArrowRegistry* reg = (KryArrowRegistry*)calloc(1, sizeof(KryArrowRegistry));
    return reg;
}

static void free_arrow_def(KryArrowDef* def) {
    if (!def) return;

    free(def->name);
    free_string_array(def->params, def->param_count);
    free(def->body_code);
    free_string_array(def->captured_vars, def->captured_count);
    // context_hint is not owned
    free(def);
}

void kry_arrow_registry_free(KryArrowRegistry* reg) {
    if (!reg) return;

    KryArrowDef* def = reg->head;
    while (def) {
        KryArrowDef* next = def->next;
        free_arrow_def(def);
        def = next;
    }

    free(reg);
}

uint32_t kry_arrow_register(
    KryArrowRegistry* reg,
    const char** params,
    size_t param_count,
    const char* body_code,
    const char** captured_vars,
    size_t captured_count,
    const char* context_hint,
    bool has_return_value
) {
    if (!reg) return 0;

    KryArrowDef* def = (KryArrowDef*)calloc(1, sizeof(KryArrowDef));
    def->id = reg->next_id++;

    // Generate name
    char name[64];
    if (context_hint && context_hint[0]) {
        snprintf(name, sizeof(name), "kry_arrow_%u_%s", def->id, context_hint);
    } else {
        snprintf(name, sizeof(name), "kry_arrow_%u", def->id);
    }
    def->name = strdup(name);

    // Copy params
    def->params = copy_string_array(params, param_count);
    def->param_count = param_count;

    // Copy body code
    def->body_code = body_code ? strdup(body_code) : NULL;

    // Copy captured vars
    def->captured_vars = copy_string_array(captured_vars, captured_count);
    def->captured_count = captured_count;

    def->context_hint = context_hint;
    def->has_return_value = has_return_value;

    // Append to list
    if (!reg->head) {
        reg->head = def;
        reg->tail = def;
    } else {
        reg->tail->next = def;
        reg->tail = def;
    }
    reg->count++;

    return def->id;
}

KryArrowDef* kry_arrow_get(KryArrowRegistry* reg, uint32_t id) {
    if (!reg) return NULL;

    for (KryArrowDef* def = reg->head; def; def = def->next) {
        if (def->id == id) {
            return def;
        }
    }
    return NULL;
}

// ============================================================================
// Code Generation
// ============================================================================

char* kry_arrow_generate_stubs(KryArrowRegistry* reg) {
    if (!reg || !reg->head) return strdup("");

    ArrowBuffer buf;
    abuf_init(&buf);

    abuf_append(&buf, "/* ===== Arrow Function Stubs (auto-generated) ===== */\n\n");

    for (KryArrowDef* def = reg->head; def; def = def->next) {
        // Generate context struct if has captures
        if (def->captured_count > 0) {
            abuf_append(&buf, "typedef struct kry_ctx_");
            abuf_append_int(&buf, def->id);
            abuf_append(&buf, " {\n");

            for (size_t i = 0; i < def->captured_count; i++) {
                abuf_append(&buf, "    void* ");
                abuf_append(&buf, def->captured_vars[i]);
                abuf_append(&buf, ";\n");
            }

            abuf_append(&buf, "} kry_ctx_");
            abuf_append_int(&buf, def->id);
            abuf_append(&buf, ";\n\n");
        }

        // Generate static function
        abuf_append(&buf, "static ");

        // Return type
        if (def->has_return_value) {
            abuf_append(&buf, "void* ");
        } else {
            abuf_append(&buf, "void ");
        }

        abuf_append(&buf, def->name);
        abuf_append(&buf, "(");

        // Parameters
        if (def->captured_count > 0) {
            abuf_append(&buf, "void* _ctx");
            if (def->param_count > 0) {
                abuf_append(&buf, ", ");
            }
        }

        if (def->param_count > 0) {
            for (size_t i = 0; i < def->param_count; i++) {
                if (i > 0) abuf_append(&buf, ", ");
                abuf_append(&buf, "void* ");
                abuf_append(&buf, def->params[i]);
            }
        } else if (def->captured_count == 0) {
            abuf_append(&buf, "void");
        }

        abuf_append(&buf, ") {\n");

        // Unpack context if has captures
        if (def->captured_count > 0) {
            abuf_append(&buf, "    kry_ctx_");
            abuf_append_int(&buf, def->id);
            abuf_append(&buf, "* ctx = (kry_ctx_");
            abuf_append_int(&buf, def->id);
            abuf_append(&buf, "*)_ctx;\n");

            // Unpack each capture to local variable
            for (size_t i = 0; i < def->captured_count; i++) {
                abuf_append(&buf, "    void* ");
                abuf_append(&buf, def->captured_vars[i]);
                abuf_append(&buf, " = ctx->");
                abuf_append(&buf, def->captured_vars[i]);
                abuf_append(&buf, ";\n");
            }

            // Suppress unused variable warnings
            abuf_append(&buf, "    (void)ctx;\n");
        }

        // Suppress unused parameter warnings
        for (size_t i = 0; i < def->param_count; i++) {
            abuf_append(&buf, "    (void)");
            abuf_append(&buf, def->params[i]);
            abuf_append(&buf, ";\n");
        }

        // Function body
        abuf_append(&buf, "    ");
        if (def->has_return_value) {
            abuf_append(&buf, "return (void*)(intptr_t)(");
        }

        if (def->body_code && def->body_code[0]) {
            abuf_append(&buf, def->body_code);
        } else {
            abuf_append(&buf, "0");
        }

        if (def->has_return_value) {
            abuf_append(&buf, ")");
        }
        abuf_append(&buf, ";\n");

        abuf_append(&buf, "}\n\n");
    }

    return abuf_extract(&buf);
}

char* kry_arrow_generate_ctx_init(KryArrowRegistry* reg, uint32_t id) {
    KryArrowDef* def = kry_arrow_get(reg, id);
    if (!def || def->captured_count == 0) {
        return NULL;
    }

    ArrowBuffer buf;
    abuf_init(&buf);

    // Example: kry_ctx_0 _ctx_0 = {.habit = habit, .index = index}
    abuf_append(&buf, "kry_ctx_");
    abuf_append_int(&buf, def->id);
    abuf_append(&buf, " _ctx_");
    abuf_append_int(&buf, def->id);
    abuf_append(&buf, " = {");

    for (size_t i = 0; i < def->captured_count; i++) {
        if (i > 0) abuf_append(&buf, ", ");
        abuf_append(&buf, ".");
        abuf_append(&buf, def->captured_vars[i]);
        abuf_append(&buf, " = ");
        abuf_append(&buf, def->captured_vars[i]);
    }

    abuf_append(&buf, "}");

    return abuf_extract(&buf);
}

// ============================================================================
// Capture Analysis Implementation
// ============================================================================

// Helper to add unique identifier to array
static void add_unique_identifier(char*** ids, size_t* count, size_t* capacity, const char* id) {
    // Check if already present
    for (size_t i = 0; i < *count; i++) {
        if (strcmp((*ids)[i], id) == 0) {
            return;
        }
    }

    // Grow array if needed
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        *ids = (char**)realloc(*ids, (*capacity) * sizeof(char*));
    }

    (*ids)[*count] = strdup(id);
    (*count)++;
}

// Recursive identifier collection
static void collect_identifiers_recursive(
    KryExprNode* node,
    char*** ids,
    size_t* count,
    size_t* capacity
) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_IDENTIFIER:
            add_unique_identifier(ids, count, capacity, node->identifier);
            break;

        case KRY_EXPR_LITERAL:
            // Literals don't contain identifiers
            break;

        case KRY_EXPR_BINARY_OP:
            collect_identifiers_recursive(node->binary_op.left, ids, count, capacity);
            collect_identifiers_recursive(node->binary_op.right, ids, count, capacity);
            break;

        case KRY_EXPR_UNARY_OP:
            collect_identifiers_recursive(node->unary_op.operand, ids, count, capacity);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            collect_identifiers_recursive(node->property_access.object, ids, count, capacity);
            // Note: property name is not an identifier reference
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            collect_identifiers_recursive(node->element_access.array, ids, count, capacity);
            collect_identifiers_recursive(node->element_access.index, ids, count, capacity);
            break;

        case KRY_EXPR_CALL:
            collect_identifiers_recursive(node->call.callee, ids, count, capacity);
            for (size_t i = 0; i < node->call.arg_count; i++) {
                collect_identifiers_recursive(node->call.args[i], ids, count, capacity);
            }
            break;

        case KRY_EXPR_ARRAY:
            for (size_t i = 0; i < node->array.element_count; i++) {
                collect_identifiers_recursive(node->array.elements[i], ids, count, capacity);
            }
            break;

        case KRY_EXPR_OBJECT:
            for (size_t i = 0; i < node->object.prop_count; i++) {
                collect_identifiers_recursive(node->object.values[i], ids, count, capacity);
            }
            break;

        case KRY_EXPR_ARROW_FUNC:
            // Don't recurse into nested arrow functions for capture analysis
            // Nested arrows have their own scope
            break;

        case KRY_EXPR_MEMBER_EXPR:
            collect_identifiers_recursive(node->member_expr.object, ids, count, capacity);
            break;

        case KRY_EXPR_CONDITIONAL:
            collect_identifiers_recursive(node->conditional.condition, ids, count, capacity);
            collect_identifiers_recursive(node->conditional.consequent, ids, count, capacity);
            collect_identifiers_recursive(node->conditional.alternate, ids, count, capacity);
            break;
    }
}

void kry_expr_collect_identifiers(
    KryExprNode* node,
    char*** ids,
    size_t* count
) {
    *ids = NULL;
    *count = 0;
    size_t capacity = 0;

    collect_identifiers_recursive(node, ids, count, &capacity);
}

void kry_arrow_get_captures(
    KryExprNode* arrow,
    char*** captures,
    size_t* count
) {
    *captures = NULL;
    *count = 0;

    if (!arrow || arrow->type != KRY_EXPR_ARROW_FUNC) {
        return;
    }

    // Collect identifiers from body
    char** body_ids = NULL;
    size_t body_count = 0;
    kry_expr_collect_identifiers(arrow->arrow_func.body, &body_ids, &body_count);

    if (body_count == 0) {
        return;
    }

    // Filter out parameters - captured = body identifiers - params
    size_t cap_capacity = 8;
    *captures = (char**)malloc(cap_capacity * sizeof(char*));
    *count = 0;

    for (size_t i = 0; i < body_count; i++) {
        bool is_param = false;
        for (size_t j = 0; j < arrow->arrow_func.param_count; j++) {
            if (strcmp(body_ids[i], arrow->arrow_func.params[j]) == 0) {
                is_param = true;
                break;
            }
        }

        if (!is_param) {
            // This identifier is captured from outer scope
            if (*count >= cap_capacity) {
                cap_capacity *= 2;
                *captures = (char**)realloc(*captures, cap_capacity * sizeof(char*));
            }
            (*captures)[*count] = strdup(body_ids[i]);
            (*count)++;
        }
    }

    // Free body identifiers
    free_string_array(body_ids, body_count);

    // If no captures, free array
    if (*count == 0) {
        free(*captures);
        *captures = NULL;
    }
}
