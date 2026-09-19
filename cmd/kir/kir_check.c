#include "kir_check.h"
#include "kir_borrow.h"
#include "kir_text.h"
#include "kir_emit.h"
#include "kir_expr.h"
#include "kir_diagnostic.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef struct Binding {
    int is_instance;
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    int depth;
} Binding;

typedef struct Checker {
    struct Checker *parent;
    KirProgram **programs;
    int program_count;
    KirModule *module;
    KirFunction *fn;
    Binding *bindings;
    int count, capacity, depth, strict, errors, failed;
} Checker;

const char *
KirScalarType(const char *type)
{
    static const struct { const char *source, *type; } types[] = {
        {"int", "i32"}, {"unsigned int", "u32"}, {"unsigned", "u32"},
        {"int8_t", "i8"}, {"int16_t", "i16"}, {"int32_t", "i32"}, {"int64_t", "i64"},
        {"uint8_t", "u8"}, {"uint16_t", "u16"}, {"uint32_t", "u32"}, {"uint64_t", "u64"},
        {"float", "f32"}, {"double", "f64"}, {"intptr_t", "isize"}, {"uintptr_t", "usize"},
        {"size_t", "usize"}, {"ptrdiff_t", "isize"}, {"char", "char"},
        {"bool", "bool"}, {"void", "void"}, {"i8", "i8"}, {"u8", "u8"},
        {"i16", "i16"}, {"u16", "u16"}, {"i32", "i32"}, {"u32", "u32"},
        {"i64", "i64"}, {"u64", "u64"}, {"isize", "isize"}, {"usize", "usize"},
        {"f32", "f32"}, {"f64", "f64"}, {"string", "string"}, {NULL, NULL}
    };
    for(int i = 0; types[i].source; i++)
        if(!strcmp(type, types[i].source)) return types[i].type;
    return "";
}

static void
error(Checker *c, KirSourceSpan span, const char *message, const char *detail)
{
    c->errors++;
    if(!c->strict) return;
    KirDiagnostic(span, "check.type", "%s%s%s",
            message, detail && *detail ? ": " : "", detail ? detail : "");
}

static void
signature_error(Checker *c, const KirFunction *callee, KirSourceSpan span,
                const char *message, const char *name)
{
    int previous_strict = c->strict;
    /* A declared UI contract applies to ordinary calls as well as blocks.
     * Unknown host calls retain the existing permissive checking policy. */
    if(callee != NULL && callee->is_ui) {
        c->strict = 1;
        c->failed = 1;
    }
    error(c, span, message, name);
    c->strict = previous_strict;
}

static void
bind(Checker *c, const char *name, const char *type, KirSourceSpan span)
{
    if(!*name) return;
    for(int i = c->count - 1; i >= 0 && c->bindings[i].depth == c->depth; i--)
        if(!strcmp(c->bindings[i].name, name)) {
            error(c, span, "duplicate binding", name);
            return;
        }
    if(c->count == c->capacity) {
        int size = c->capacity ? c->capacity * 2 : 32;
        Binding *next = realloc(c->bindings, (size_t)size * sizeof(*next));
        if(!next) { c->errors++; c->failed=1; return; }
        c->bindings = next; c->capacity = size;
    }
    kir_copy(c->bindings[c->count].name, KIR_NAME_MAX, name);
    kir_copy(c->bindings[c->count].type, KIR_NAME_MAX, type);
    c->bindings[c->count].is_instance = 0;
    c->bindings[c->count++].depth = c->depth;
}

static const KirFunction *
function(Checker *c, const char *name, KirSourceSpan span)
{
    const KirModule *owner = NULL;
    const KirFunction *found = NULL;
    if(KirResolveFunction(c->module, name, &owner, &found) < 0) {
        error(c, span, "ambiguous imported function", name);
        c->failed = 1;
    }
    return found;
}

static int
lexical_instance(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!strcmp(c->bindings[i].name, name))
            return c->bindings[i].is_instance;
    return c->parent ? lexical_instance(c->parent, name) : 0;
}

static const char *
lookup_lexical(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!strcmp(c->bindings[i].name, name))
            return c->bindings[i].type;
    if(c->parent == NULL)
        return "";
    const char *type = lookup_lexical(c->parent, name);
    if(!*type)
        return "";
    for(int i = 0; i < c->fn->capture_count; i++)
        if(!strcmp(c->fn->captures[i].name, name))
            return c->fn->captures[i].type;
    KirCapture *captures = realloc(c->fn->captures,
        (size_t)(c->fn->capture_count + 1) * sizeof(*captures));
    if(captures == NULL) {
        c->failed = 1;
        return "";
    }
    c->fn->captures = captures;
    KirCapture *capture = &captures[c->fn->capture_count++];
    kir_copy(capture->name, sizeof(capture->name), name);
    kir_copy(capture->type, sizeof(capture->type), type);
    capture->is_instance = lexical_instance(c->parent, name);
    return capture->type;
}

static const char *
lookup(Checker *c, const char *name)
{
    const char *lexical = lookup_lexical(c, name);
    if(*lexical)
        return lexical;
    for(int i = 0; i < c->module->state_count; i++)
        if(!strcmp(c->module->state_fields[i].name, name)) return c->module->state_fields[i].type;
    for(int i = 0; i < c->module->global_count; i++)
        if(!strcmp(c->module->globals[i].name, name)) return c->module->globals[i].type;
    const KirModule *owner = NULL;
    const KirType *type = NULL;
    int resolved = KirResolveEnumMember(c->module, name, &owner, &type);
    if(resolved < 0) {
        error(c, c->fn->span, "ambiguous enum member", name);
        c->failed = 1;
    }
    if(resolved > 0)
        return "integer";
    return "";
}

static int
numeric(const char *type)
{
    return !strcmp(type, "integer") || !strcmp(type, "real") ||
           (*type && strchr("iuf", type[0]) && *KirScalarType(type)) || !strcmp(type, "char");
}

static int
integer_type(const char *type)
{
    const char *scalar = KirScalarType(type);
    return !strcmp(type, "integer") || !strcmp(scalar, "char") ||
           scalar[0] == 'i' || scalar[0] == 'u';
}

/* Bounds use a checked integer subset: every intermediate must fit i32.
 * This avoids accepting a size whose C constant expression overflows while
 * Go evaluates it with arbitrary precision. Unknown host macros stay opaque. */
static int bound_constant(const KirModule *module, const char *name, int depth, int64_t *value);

static int
bound_expression(const KirModule *module, const KirFunction *expression, int index,
                 int depth, int64_t *value)
{
    if(index < 0 || depth > 128)
        return -1;
    const KirExpr *node = &expression->exprs[index];
    if(node->kind == KIR_EXPR_IDENT)
        return bound_constant(module, node->name, depth + 1, value);
    if(node->kind == KIR_EXPR_INT) {
        char *end;
        errno = 0;
        *value = strtoll(node->text, &end, 0);
        if(errno || end == node->text || *end || *value < 0 || *value > INT32_MAX)
            return -1;
        return 1;
    }
    int64_t left = 0, right = 0;
    if(node->kind == KIR_EXPR_UNARY) {
        int status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(!strcmp(node->op, "+"))
            *value = right;
        else if(!strcmp(node->op, "-"))
            *value = -right;
        else
            return -1;
    } else if(node->kind == KIR_EXPR_BINARY) {
        int status = bound_expression(module, expression, node->left, depth + 1, &left);
        if(status != 1)
            return status;
        status = bound_expression(module, expression, node->right, depth + 1, &right);
        if(status != 1)
            return status;
        if(left == INT32_MIN && right == -1 &&
           (!strcmp(node->op, "/") || !strcmp(node->op, "%")))
            return -1;
        if(!strcmp(node->op, "+"))
            *value = left + right;
        else if(!strcmp(node->op, "-"))
            *value = left - right;
        else if(!strcmp(node->op, "*"))
            *value = left * right;
        else if(!strcmp(node->op, "/") && right != 0)
            *value = left / right;
        else if(!strcmp(node->op, "%") && right != 0)
            *value = left % right;
        else
            return -1;
    } else {
        return -1;
    }
    return *value >= INT32_MIN && *value <= INT32_MAX ? 1 : -1;
}

static int
bound_constant(const KirModule *module, const char *name, int depth, int64_t *value)
{
    if(depth > 128)
        return -1;
    const KirDefine *definition = NULL;
    const KirModule *owner = NULL;
    for(int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const KirModule *scope = pass == 0 ? module : module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                const KirDefine *candidate = &scope->defines[j];
                if(strcmp(candidate->name, name))
                    continue;
                if(definition != NULL && definition != candidate)
                    return -1;
                definition = candidate;
                owner = scope;
            }
        }
        if(definition != NULL)
            break;
    }
    if(definition == NULL)
        return 0;
    KirFunction expression = {0};
    int index = KirParseExpr(&expression, owner, definition->value, definition->span);
    int status = bound_expression(owner, &expression, index, depth + 1, value);
    free(expression.exprs);
    return status;
}

static int
array_capacity(const KirModule *module, const char *type, int *capacity)
{
    if(!KirArrayElementType(type, NULL, 0, capacity))
        return -1;
    if(*capacity >= 0)
        return 1;
    char name[KIR_NAME_MAX];
    size_t length = (size_t)(strchr(type, ']') - type - 1);
    memcpy(name, type + 1, length);
    name[length] = '\0';
    int64_t value = 0;
    int status = bound_constant(module, name, 0, &value);
    if(status != 1)
        return status;
    if(value < 1 || value > 1048576)
        return -1;
    *capacity = (int)value;
    return 1;
}

static void
normalize_array(const KirModule *module, char *type, size_t size)
{
    char element[KIR_NAME_MAX];
    int capacity;
    if(KirArrayElementType(type, element, sizeof(element), NULL) &&
       array_capacity(module, type, &capacity) == 1) {
        char normalized[KIR_NAME_MAX];
        int length = snprintf(normalized, sizeof(normalized), "[%d]%s", capacity, element);
        if(length >= 0 && (size_t)length < sizeof(normalized))
            kir_copy(type, size, normalized);
    }
}

static int
compatible(const char *to, const char *from)
{
    const char *canonical = KirScalarType(to);
    if(*canonical) to = canonical;
    if(!*to || !*from) return 1;
    if(!strcmp(to, from)) return 1;
    if(KirSliceElementType(to, NULL, 0) || KirSliceElementType(from, NULL, 0)) {
        char a[KIR_NAME_MAX], b[KIR_NAME_MAX];
        if(!KirSliceElementType(to, a, sizeof(a)) || !KirSliceElementType(from, b, sizeof(b)))
            return 0;
        const char *ca = KirScalarType(a), *cb = KirScalarType(b);
        return !strcmp(*ca ? ca : a, *cb ? cb : b);
    }
    if(to[0] == '[' || from[0] == '[') {
        char to_element[KIR_NAME_MAX], from_element[KIR_NAME_MAX];
        int to_capacity, from_capacity;
        if(!KirArrayElementType(to, to_element, sizeof(to_element), &to_capacity) ||
           !KirArrayElementType(from, from_element, sizeof(from_element), &from_capacity))
            return 0;
        if(to_capacity != from_capacity)
            return 0;
        if(to_capacity < 0) {
            size_t length = (size_t)(strchr(to, ']') - to);
            if(length != (size_t)(strchr(from, ']') - from) || strncmp(to, from, length))
                return 0;
        }
        const char *to_scalar = KirScalarType(to_element);
        const char *from_scalar = KirScalarType(from_element);
        return !strcmp(*to_scalar ? to_scalar : to_element,
                       *from_scalar ? from_scalar : from_element);
    }
    if(!strcmp(to, "const char*") && !strcmp(from, "string")) return 1;
    if(!strcmp(from, "integer") && numeric(to)) return 1;
    if(!strcmp(from, "real") && (to[0] == 'f')) return 1;
    return 0;
}

static int
text_type(const char *type)
{
    return !strcmp(type, "string") || !strcmp(type, "const char*");
}

static void
check_borrowed_string(Checker *c, const char *destination, int index)
{
    if(index < 0 || strcmp(destination, "const char*"))
        return;
    const KirExpr *value = &c->fn->exprs[index];
    if(!strcmp(value->type, "string") && value->kind != KIR_EXPR_STRING)
        error(c, value->span, "borrowed string requires a literal or another borrowed value", "");
}

static int
assignable(Checker *c, int index)
{
    const KirExpr *e;
    if(index < 0 || index >= c->fn->expr_count) return 0;
    e = &c->fn->exprs[index];
    return (e->kind == KIR_EXPR_IDENT && strcmp(e->name, "true") && strcmp(e->name, "false")) ||
           e->kind == KIR_EXPR_INDEX || e->kind == KIR_EXPR_MEMBER ||
           e->kind == KIR_EXPR_POINTER_MEMBER || (e->kind == KIR_EXPR_UNARY && !strcmp(e->op, "*"));
}

/* String bytes and the string length member are read-only views. */
static int
readonly_text_destination(const KirFunction *fn, int index)
{
    const KirExpr *e;
    if(index < 0 || index >= fn->expr_count)
        return 0;
    e = &fn->exprs[index];
    if(e->kind == KIR_EXPR_MEMBER && !strcmp(e->name, "length") &&
       KirSliceElementType(fn->exprs[e->left].type, NULL, 0))
        return 1;
    if(e->kind == KIR_EXPR_INDEX || e->kind == KIR_EXPR_MEMBER) {
        if(fn->exprs[e->left].kind == KIR_EXPR_IDENT &&
           !strcmp(fn->exprs[e->left].type, "string"))
            return 1;
        return readonly_text_destination(fn, e->left);
    }
    return 0;
}

static int check_function(Checker *c, KirFunction *fn);

/* Function values require a slot context; ordinary names retain lexical lookup.
 * Annotate before recursively checking expressions so a declaration identifier
 * is not mistaken for an unresolved variable. */
static void
contextual_slot(Checker *c, int index, const char *expected)
{
    const KirModule *slot_owner = NULL;
    const KirType *slot = KirFindType(c->module, expected, &slot_owner);
    if(index < 0 || slot == NULL || !slot->is_slot)
        return;
    KirExpr *value = &c->fn->exprs[index];
    if(value->kind == KIR_EXPR_CONDITIONAL) {
        contextual_slot(c, value->right, expected);
        contextual_slot(c, value->third, expected);
        return;
    }
    if(value->kind != KIR_EXPR_IDENT || *lookup(c, value->name))
        return;
    const KirModule *owner = NULL;
    const KirFunction *declaration = NULL;
    int resolved = KirResolveFunction(c->module, value->name, &owner, &declaration);
    int matches = resolved == 1 && !declaration->is_extern &&
                  !strcmp(declaration->return_type, "void");
    char actual[64][KIR_TEXT_MAX], wanted[64][KIR_TEXT_MAX];
    int actual_count = matches && *kir_skip_ws(declaration->args) ?
        kir_split_top(declaration->args, actual[0], 64, sizeof(actual[0])) : 0;
    int wanted_count = *kir_skip_ws(slot->body) ?
        kir_split_top(slot->body, wanted[0], 64, sizeof(wanted[0])) : 0;
    matches &= actual_count == wanted_count;
    for(int i = 0; matches && i < wanted_count; i++) {
        const char *actual_type = strchr(actual[i], ':');
        const char *wanted_type = strchr(wanted[i], ':');
        if(actual_type == NULL || wanted_type == NULL) {
            matches = 0;
            break;
        }
        actual_type = kir_skip_ws(actual_type + 1);
        wanted_type = kir_skip_ws(wanted_type + 1);
        const char *actual_scalar = KirScalarType(actual_type);
        const char *wanted_scalar = KirScalarType(wanted_type);
        if(*actual_scalar || *wanted_scalar) {
            matches = !strcmp(actual_scalar, wanted_scalar);
        } else {
            const KirType *actual_record = KirFindType(owner, actual_type, NULL);
            const KirType *wanted_record = KirFindType(slot_owner, wanted_type, NULL);
            matches = actual_record || wanted_record ? actual_record == wanted_record :
                !strcmp(actual_type, wanted_type);
        }
    }
    if(!matches) {
        KirDiagnostic(value->span, "check.slot_signature",
                      "function does not match slot signature %s: %s", expected, value->name);
        c->errors++;
        c->failed = 1;
        return;
    }
    if(declaration->is_closure && !declaration->checked) {
        Checker child = *c;
        child.parent = c;
        child.bindings = NULL;
        child.count = child.capacity = 0;
        child.errors = child.failed = 0;
        child.strict = 1;
        if(!check_function(&child, (KirFunction *)declaration))
            child.failed = 1;
        c->errors += child.errors;
        c->failed |= child.failed;
        free(child.bindings);
    }
    value->is_function_value = 1;
    kir_copy(value->type, sizeof(value->type), expected);
}

/* Modules that import C headers interoperate with C: calls and names the
 * frontend cannot see are verified by the C compiler, not by strict checking.
 * A quoted import that resolved to another Kry module is not C interop. */
static int
module_uses_c(const Checker *c)
{
    for(int i = 0; i < c->module->import_count; i++)
        if(c->module->imports[i].kind == KIR_IMPORT_HEADER &&
           c->module->imports[i].resolved_module == NULL)
            return 1;
    return 0;
}

static const char *local_storage_error(const KirModule *module, const char *type);

static const char *
expression_type(Checker *c, int index)
{
    KirExpr *e;
    const char *type = "", *left = "", *right = "";
    char member_type[KIR_NAME_MAX] = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->is_function_value)
        return e->type;
    if(e->left >= 0) left = expression_type(c, e->left);
    if(e->right >= 0) right = expression_type(c, e->right);
    switch(e->kind) {
    case KIR_EXPR_COMPOUND: {
        if(KirSliceElementType(e->name, NULL, 0)) {
            error(c, e->span, "slice literals require a backing range", e->name);
            break;
        }
        if(e->name[0] == '[') {
            char element[KIR_NAME_MAX];
            int capacity = 0;
            const char *problem = local_storage_error(c->module, e->name);
            if(problem != NULL) {
                error(c, e->span, problem, e->name);
                break;
            }
            normalize_array(c->module, e->name, sizeof(e->name));
            KirArrayElementType(e->name, element, sizeof(element), &capacity);
            if(capacity < 0)
                error(c, e->span, "array literals require a resolved capacity", e->name);
            int count = 0;
            for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
                KirExpr *entry = &c->fn->exprs[child];
                if(!strcmp(entry->op, "="))
                    error(c, entry->span, "array literals require positional elements", entry->name);
                const char *value_type = expression_type(c, entry->right);
                if(!compatible(element, value_type))
                    error(c, entry->span, "array initializer element type mismatch", element);
                check_borrowed_string(c, element, entry->right);
                kir_copy(entry->type, sizeof(entry->type), element);
                count++;
            }
            if(capacity >= 0 && count > capacity)
                error(c, e->span, "too many array initializer elements", e->name);
            type = e->name;
            break;
        }
        const KirModule *record_owner = NULL;
        const KirType *record = KirFindType(c->module, e->name, &record_owner);
        int ordinal = 0;
        int mode = -1;
        if(record == NULL || record->is_enum || record->is_slot) {
            error(c, e->span, "initializer requires a declared record type", e->name);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            KirExpr *entry = &c->fn->exprs[child];
            int named = !strcmp(entry->op, "=");
            if(mode >= 0 && mode != named)
                error(c, entry->span, "cannot mix named and positional record fields", e->name);
            mode = named;
            size_t offset = 0;
            KirTypeField field;
            int position = 0;
            int found = 0;
            while(KirTypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) : position == ordinal) {
                    found = 1;
                    break;
                }
                position++;
            }
            if(found) {
                normalize_array(record_owner, field.type, sizeof(field.type));
                KirExpr *initializer = &c->fn->exprs[entry->right];
                if(initializer->kind == KIR_EXPR_COMPOUND)
                    normalize_array(record_owner, initializer->name, sizeof(initializer->name));
            }
            const char *value_type = expression_type(c, entry->right);
            if(!found) {
                error(c, entry->span, named ? "unknown initializer field" : "too many positional record fields", entry->name);
                ordinal++;
                continue;
            }
            for(int previous = e->first_child; previous != child; previous = c->fn->exprs[previous].next_sibling) {
                if(!strcmp(c->fn->exprs[previous].name, field.name))
                    error(c, entry->span, "duplicate initializer field", field.name);
            }
            kir_copy(entry->name, sizeof(entry->name), field.name);
            kir_copy(entry->type, sizeof(entry->type), field.type);
            if(!compatible(field.type, value_type))
                error(c, entry->span, "initializer field type mismatch", field.name);
            check_borrowed_string(c, field.type, entry->right);
            ordinal++;
        }
        type = e->name;
        break;
    }
    case KIR_EXPR_MEMBER: {
        const KirModule *record_owner = NULL;
        const KirType *record = KirFindType(c->module, left, &record_owner);
        if(record == NULL && (!strcmp(left, "string") || KirSliceElementType(left, NULL, 0)) &&
           !strcmp(e->name, "length")) {
            /* Byte length of a borrowed string value; read-only. */
            type = "i32";
            break;
        }
        if(record != NULL && !record->is_enum) {
            size_t offset = 0;
            KirTypeField field;
            while(KirTypeNextField(record, &offset, &field) == 1) {
                if(!strcmp(field.name, e->name)) {
                    kir_copy(member_type, sizeof(member_type), field.type);
                    normalize_array(record_owner, member_type, sizeof(member_type));
                    break;
                }
            }
        }
        if(!*member_type) error(c, e->span, "unknown record field", e->name);
        type = member_type;
        break;
    }
    case KIR_EXPR_SLICE: {
        char element[KIR_NAME_MAX];
        int array = KirArrayElementType(left, element, sizeof(element), NULL);
        if(!array && !KirSliceElementType(left, element, sizeof(element))) {
            error(c, e->span, "slice source requires an array or slice", e->text);
            break;
        }
        if(array && !assignable(c, e->left))
            error(c, e->span, "slice source requires persistent array storage", e->text);
        if((!strcmp(element, "char") || !strcmp(element, "const char")) || element[0] == '[')
            error(c, e->span, "unsupported slice element type", element);
        if(e->right >= 0 && !integer_type(right))
            error(c, e->span, "slice lower bound requires an integer", e->text);
        if(e->third >= 0 && !integer_type(expression_type(c, e->third)))
            error(c, e->span, "slice upper bound requires an integer", e->text);
        snprintf(member_type, sizeof(member_type), "[]%s", element);
        type = member_type;
        break;
    }
    case KIR_EXPR_INDEX: {
        char element[KIR_NAME_MAX];

        /* 'base[index]': a fixed-capacity array element, or a read-only
         * byte of a borrowed string. Element types stay scalar so every
         * backend lowers the same shape. */
        if(!strcmp(left, "string")) {
            if(!integer_type(right))
                error(c, e->span, "string index requires an integer operand", e->text);
            kir_copy(element, sizeof(element), "u8");
        } else if(!KirArrayElementType(left, element, sizeof(element), NULL) &&
                  !KirSliceElementType(left, element, sizeof(element))) {
            error(c, e->span, "index requires a fixed array or string", e->text);
            kir_copy(element, sizeof(element), "i32");
        } else if(!integer_type(right)) {
            error(c, e->span, "array index requires an integer operand", e->text);
        }
        kir_copy(member_type, sizeof(member_type), element);
        type = member_type;
        break;
    }
    case KIR_EXPR_INT: type = "integer"; break;
    case KIR_EXPR_FLOAT: type = "real"; break;
    case KIR_EXPR_CHAR: type = "char"; break;
    case KIR_EXPR_STRING: type = "string"; break;
    case KIR_EXPR_IDENT:
        if(!strcmp(e->name, "true") || !strcmp(e->name, "false")) type = "bool";
        else type = lookup(c, e->name);
        if(!*type && KirFindRuntimeEnumMember(e->name) != NULL)
            type = "integer";   /* runtime enum member; the emitter resolves it */
        if(!*type) error(c, e->span, "unresolved name", e->name);
        break;
    case KIR_EXPR_CALL: {
        const char *binding = lookup(c, e->name);
        const KirType *slot = KirFindType(c->module, binding, NULL);
        if(slot != NULL && !slot->is_slot)
            slot = NULL;
        kir_copy(e->slot_type, sizeof(e->slot_type), slot ? binding : "");
        const KirFunction *callee = *binding ? NULL : function(c, e->name, e->span);
        if(callee != NULL && callee->is_extern && callee->extern_kind == KIR_EXTERN_HOST)
            c->fn->uses_host = 1;
        const char *args = slot ? slot->body : callee ? callee->args : NULL;
        const char *return_type = slot ? "void" : callee ? callee->return_type : "";
        char (*parts)[KIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        int strict_before_slot = c->strict;
        int errors_before_slot = c->errors;
        int slot_contract = slot != NULL;
        if(slot_contract)
            c->strict = 1;
        if(!parts) { c->errors++; c->failed=1; break; }
        if(*binding && slot == NULL)
            error(c, e->span, "binding is not a callable function", e->name);
        if(!callee && slot == NULL) for(int i = 0; i < c->module->import_count; i++) {
            const KirImport *imp = &c->module->imports[i];
            if(imp->kind == KIR_IMPORT_EXTERN && !strcmp(imp->name, e->name)) {
                if(imp->extern_kind == KIR_EXTERN_HOST)
                    c->fn->uses_host = 1;
                args = imp->args; return_type = imp->return_type; break;
            }
        }
        expected = args && *kir_skip_ws(args) ? kir_split_top(args, parts[0], 64, sizeof(parts[0])) : 0;
        const KirModule *signature_owner = c->module;
        if(callee != NULL) {
            const KirFunction *resolved = NULL;
            KirResolveFunction(c->module, e->name, &signature_owner, &resolved);
        }
        for(int parameter = 0; parameter < expected; parameter++) {
            const char *colon = strchr(parts[parameter], ':');
            const KirType *parameter_type = colon ?
                KirFindType(signature_owner, kir_skip_ws(colon + 1), NULL) : NULL;
            slot_contract |= parameter_type != NULL && parameter_type->is_slot;
        }
        if(slot_contract)
            c->strict = 1;
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            const char *expected_type = actual < expected ? strchr(parts[actual], ':') : NULL;
            if(expected_type != NULL)
                contextual_slot(c, child, kir_skip_ws(expected_type + 1));
            const char *arg_type = expression_type(c, child);
            if(args && actual < expected) {
                char *colon = strchr(parts[actual], ':');
                if(colon)
                    check_borrowed_string(c, kir_skip_ws(colon + 1), child);
                if(colon && !compatible(kir_skip_ws(colon + 1), arg_type))
                    signature_error(c, callee, c->fn->exprs[child].span,
                                    "argument type mismatch", e->name);
                if(colon && (!strcmp(arg_type, "integer") || !strcmp(arg_type, "real"))) {
                    const char *context = KirScalarType(kir_skip_ws(colon + 1));
                    if(*context) kir_copy(c->fn->exprs[child].type, KIR_NAME_MAX, context);
                }
                if(colon && c->fn->exprs[child].kind == KIR_EXPR_STRING &&
                   !strcmp(kir_skip_ws(colon + 1), "const char*"))
                    kir_copy(c->fn->exprs[child].type, KIR_NAME_MAX, "const char*");
            }
            actual++;
        }
        if(args) {
            type = return_type;
            if(actual != expected)
                signature_error(c, callee, e->span, "argument count mismatch", e->name);
        } else if(!module_uses_c(c))
            error(c, e->span, "unresolved function", e->name);
        if(slot_contract && c->errors != errors_before_slot)
            c->failed = 1;
        c->strict = strict_before_slot;
        free(parts);
        break;
    }
    case KIR_EXPR_BINARY: {
        if(left[0] == '[' || right[0] == '[')
            error(c, e->span, "array values do not support binary operations", e->op);
        const KirType *left_slot = KirFindType(c->module, left, NULL);
        const KirType *right_slot = KirFindType(c->module, right, NULL);
        if((left_slot && left_slot->is_slot) || (right_slot && right_slot->is_slot))
            error(c, e->span, "slot values do not support binary operations", e->op);
        if((text_type(left) || text_type(right)) &&
           strcmp(e->op, "==") && strcmp(e->op, "!="))
            error(c, e->span, "string operation is not supported", e->op);
        check_borrowed_string(c, left, e->right);
        check_borrowed_string(c, right, e->left);
        if((!strcmp(e->op, "&") || !strcmp(e->op, "|") || !strcmp(e->op, "^") ||
            !strcmp(e->op, "<<") || !strcmp(e->op, ">>") || !strcmp(e->op, "%")) &&
           (left[0] == 'f' || right[0] == 'f' || !strcmp(left, "real") || !strcmp(right, "real")))
            error(c, e->span, "integer operands required", e->op);
        if((!strcmp(e->op, "&&") || !strcmp(e->op, "||")) &&
           (strcmp(left, "bool") || strcmp(right, "bool")))
            error(c, e->span, "logical operands require bool", e->op);
        if(!compatible(left, right) && !compatible(right, left))
            error(c, e->span, "operand types differ; use an explicit cast", e->op);
        if(!strcmp(e->op, "==") || !strcmp(e->op, "!=") || !strcmp(e->op, "<") ||
           !strcmp(e->op, "<=") || !strcmp(e->op, ">") || !strcmp(e->op, ">=") ||
           !strcmp(e->op, "&&") || !strcmp(e->op, "||")) type = "bool";
        else if(numeric(left) && numeric(right))
            type = (!strcmp(left, "integer") || !strcmp(left, "real")) ? right : left;
        else if(*left && *right) error(c, e->span, "numeric operands required", e->op);
        break;
    }
    case KIR_EXPR_UNARY:
        if(!strcmp(e->op, "!") && strcmp(right, "bool"))
            error(c, e->span, "logical operand requires bool", e->op);
        if((!strcmp(e->op, "++") || !strcmp(e->op, "--")) && !assignable(c, e->right))
            error(c, e->span, "increment requires an assignable expression", "");
        if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else if(!module_uses_c(c))
            error(c, e->span, "unresolved unary operation", e->op);
        break;
    case KIR_EXPR_POSTFIX:
        if(!numeric(left)) error(c, e->span, "increment requires a numeric value", e->op);
        if(!assignable(c, e->left)) error(c, e->span, "increment requires an assignable expression", "");
        type = left;
        break;
    case KIR_EXPR_CAST: {
        if(right[0] == '[' || e->name[0] == '[')
            error(c, e->span, "array casts are not supported", e->name);
        const KirType *destination = KirFindType(c->module, e->name, NULL);
        const KirType *source = KirFindType(c->module, right, NULL);
        if(destination != NULL && destination->is_enum &&
           !numeric(right) && strcmp(right, "bool") &&
           (source == NULL || !source->is_enum))
            error(c, e->span, "enum casts require a numeric, bool, or enum value", e->name);
        if((text_type(right) || text_type(e->name)) && strcmp(right, e->name))
            error(c, e->span, "string casts require an explicit conversion API", e->name);
        type = e->name;
        break;
    }
    case KIR_EXPR_CONDITIONAL: {
        const char *third = expression_type(c, e->third);
        if(strcmp(left, "bool")) error(c, e->span, "conditional requires bool", left);
        if(!compatible(right, third) && !compatible(third, right))
            error(c, e->span, "conditional arms have different types", "");
        type = !strcmp(right, "integer") ? third : right;
        if(!strcmp(right, "const char*") || !strcmp(third, "const char*")) {
            type = "const char*";
            check_borrowed_string(c, type, e->right);
            check_borrowed_string(c, type, e->third);
        }
        break;
    }
    default: error(c, e->span, "expression is not supported by strict checking", e->text); break;
    }
    if(*KirScalarType(type)) type = KirScalarType(type);
    kir_copy(e->type, sizeof(e->type), type);
    normalize_array(c->module, e->type, sizeof(e->type));
    return e->type;
}

static int
record_declaration_error(const KirType *record, const char *message,
                         const char *field)
{
    KirDiagnostic(record->span, "check.record", "%s: %s%s%s",
            message, record->name, field && *field ? "." : "",
            field ? field : "");
    return 0;
}

/* Structural errors are invalid in every backend, including non-strict mode.
 * Check them before function eligibility can select a fallback emitter. */
typedef struct RecordPath {
    const KirType *record;
    const struct RecordPath *parent;
    int depth;
} RecordPath;

typedef struct ValidatedRecords {
    const KirType **items;
    size_t count;
} ValidatedRecords;

static int
has_foreign_types(const KirModule *module)
{
    for(int i = 0; i < module->import_count; i++) {
        const KirImport *import = &module->imports[i];
        if(import->kind == KIR_IMPORT_HEADER && import->resolved_module == NULL)
            return 1;
    }
    return 0;
}

/* Validate stored shapes before choosing an emitter. In particular, a slice
 * must never be mistaken for a fixed array, nor a recursive value layout for
 * an opaque host type. Pointers stop layout recursion but still name a type. */
static const char *
storage_type_error(const KirModule *module, const char *source,
                   const RecordPath *path, int indirect, ValidatedRecords *checked)
{
    char type[KIR_NAME_MAX];
    char element[KIR_NAME_MAX];
    const KirModule *owner = NULL;
    const KirType *record;
    int capacity;

    kir_copy(type, sizeof(type), source);
    kir_trim_in_place(type);
    if(!strcmp(type, "void"))
        return indirect ? NULL : "stored values cannot have void type";
    if(KirSliceElementType(type, NULL, 0))
        return "slice descriptors cannot be stored in aggregates or globals";
    if(*KirScalarType(type) != '\0' || KirTargetType(type, KIR_C) != NULL)
        return NULL;
    if(type[0] == '[') {
        if(type[1] == ']')
            return "slices do not yet have portable storage semantics";
        if(!KirArrayElementType(type, element, sizeof(element), &capacity))
            return "malformed fixed array type";
        if(capacity == 0)
            return "fixed arrays require a positive capacity";
        if(capacity < 0) {
            int status = array_capacity(module, type, &capacity);
            if(status < 0)
                return "array capacity is not a valid bounded integer constant";
            if(status == 0 && !has_foreign_types(module))
                return "array capacity requires a known integer constant";
        }
        if(element[0] == '[')
            return "nested fixed arrays are not supported";
        return storage_type_error(module, element, path, indirect, checked);
    }
    size_t length = strlen(type);
    if(length > 0 && type[length - 1] == '*') {
        type[length - 1] = '\0';
        kir_trim_in_place(type);
        if(!strncmp(type, "const ", 6))
            memmove(type, type + 6, strlen(type + 6) + 1);
        return storage_type_error(module, type, path, 1, checked);
    }
    record = KirFindType(module, type, &owner);
    if(record == NULL)
        return has_foreign_types(module) ? NULL : "unknown stored type";
    if(record->is_slot)
        return "slot values cannot be stored in aggregates or pointers";
    if(record->is_enum || indirect)
        return NULL;
    for(const RecordPath *ancestor = path; ancestor != NULL; ancestor = ancestor->parent) {
        if(ancestor->record == record)
            return "record has a recursive value layout";
    }
    for(size_t i = 0; i < checked->count; i++) {
        if(checked->items[i] == record)
            return NULL;
    }
    if(path != NULL && path->depth >= 128)
        return "record nesting exceeds the portable limit";
    RecordPath current = {record, path, path == NULL ? 1 : path->depth + 1};
    KirTypeField field;
    size_t offset = 0;
    int status;
    while((status = KirTypeNextField(record, &offset, &field)) == 1) {
        const char *error = storage_type_error(owner, field.type, &current, 0, checked);
        if(error != NULL)
            return error;
    }
    if(status < 0)
        return "malformed record field";
    const KirType **items = realloc(checked->items, (checked->count + 1) * sizeof(*items));
    if(items == NULL)
        return "cannot allocate record type validation state";
    checked->items = items;
    checked->items[checked->count++] = record;
    return NULL;
}

static const char *
local_storage_error(const KirModule *module, const char *type)
{
    ValidatedRecords checked = {0};
    char element[KIR_NAME_MAX];
    int slice = KirSliceElementType(type, element, sizeof(element));
    const char *problem;
    if(slice && (element[0] == '[' || !strcmp(element, "char") || !strcmp(element, "const char")))
        problem = "unsupported slice element type";
    else
        problem = storage_type_error(module, slice ? element : type, NULL, 0, &checked);
    free(checked.items);
    return problem;
}

static int
check_type_declarations(const KirModule *module, int strict)
{
    for(int i = 0; i < module->type_count; i++) {
        const KirType *record = &module->types[i];
        size_t offset = 0;
        int status;
        KirTypeField field;

        /* Anonymous enum groups and foreign typedef payloads are not records. */
        if(record->name[0] == '#')
            continue;
        for(int previous = 0; previous < i; previous++) {
            const KirType *other = &module->types[previous];
            if(strcmp(record->name, other->name) == 0 &&
               strcmp(record->guard, other->guard) == 0)
                return record_declaration_error(record, "duplicate type declaration", NULL);
        }
        if(record->is_enum)
            continue;
        if(record->is_slot) {
            char parameters[64][KIR_TEXT_MAX];
            int count = *kir_skip_ws(record->body) ?
                kir_split_top(record->body, parameters[0], 64, sizeof(parameters[0])) : 0;
            for(int parameter = 0; parameter < count; parameter++) {
                char *colon = strchr(parameters[parameter], ':');
                if(colon == NULL)
                    return record_declaration_error(record, "slot parameters require name: type", NULL);
                *colon++ = '\0';
                kir_trim_in_place(parameters[parameter]);
                kir_trim_in_place(colon);
                const KirType *type = KirFindType(module, colon, NULL);
                if(!*parameters[parameter] || !strcmp(colon, "void") ||
                   (KirTargetType(colon, KIR_C) == NULL && type == NULL) || (type && type->is_slot))
                    return record_declaration_error(record, "invalid slot parameter", parameters[parameter]);
                for(int previous = 0; previous < parameter; previous++)
                    if(!strcmp(parameters[previous], parameters[parameter]))
                        return record_declaration_error(record, "duplicate slot parameter", parameters[parameter]);
            }
            continue;
        }
        while((status = KirTypeNextField(record, &offset, &field)) == 1) {
            size_t previous_offset = 0;
            KirTypeField previous;

            if(strstr(field.type, "[]") != NULL)
                return record_declaration_error(record, "slice descriptors cannot be stored in aggregates", field.name);
            if(strcmp(field.type, "void") == 0)
                return record_declaration_error(record, "record field cannot have void type", field.name);
            const KirType *field_type = KirFindType(module, field.type, NULL);
            if(field_type != NULL && field_type->is_slot)
                return record_declaration_error(record, "slot values cannot be stored in records", field.name);
            while(KirTypeNextField(record, &previous_offset, &previous) == 1 &&
                  previous_offset < offset) {
                if(strcmp(previous.name, field.name) == 0)
                    return record_declaration_error(record, "duplicate record field", field.name);
            }
        }
        if(status < 0)
            return record_declaration_error(record, "malformed record field", NULL);
        if(strict) {
            ValidatedRecords checked = {0};
            const char *error = storage_type_error(module, record->name, NULL, 0, &checked);
            free(checked.items);
            if(error != NULL)
                return record_declaration_error(record, error, NULL);
        }
    }
    return 1;
}

static KirStmt *
widget_statement(KirFunction *body, KirStmtKind kind, KirSourceSpan span,
                  const char *format, ...)
{
    char text[KIR_TEXT_MAX];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if(length < 0 || (size_t)length >= sizeof(text))
        return NULL;
    KirStmt *statement = KirFunctionAddStmt(body, kind, text, "", span);
    if(statement == NULL)
        return NULL;
    statement->declared_widget = 1;
    return statement;
}

static void
inherit_widget_block_metadata(KirStmt *statement, const KirStmt *source)
{
    char text[KIR_TEXT_MAX];
    char args[KIR_TEXT_MAX];
    KirStmtKind kind;

    if(statement == NULL || source == NULL)
        return;
    kind = statement->kind;
    kir_copy(text, sizeof(text), statement->text);
    kir_copy(args, sizeof(args), statement->args);
    *statement = *source;
    statement->kind = kind;
    kir_copy(statement->text, sizeof(statement->text), text);
    kir_copy(statement->args, sizeof(statement->args), args);
    statement->declared_widget = 1;
    statement->widget_fallback = 0;
    statement->expr_root = -1;
    statement->lhs_root = -1;
}

/* A block initializes props and callable arguments in source order, then calls
 * the same declaration as ordinary function syntax. Captured child bodies can
 * later enter this path as slot values without another backend widget path. */
static int
lower_widget_block(Checker *c, int index, const KirModule *owner,
                      const KirType *props, const char *temporary,
                      char parameters[][KIR_TEXT_MAX], int parameter_count)
{
    const KirStmt *source = &c->fn->stmts[index];
    KirSourceSpan span = source->span;
    KirFunction lowered = {0};
    char fields[64][KIR_TEXT_MAX];
    char slot_names[64][KIR_NAME_MAX] = {{0}};
    const char *slot_types[64] = {0};
    int supplied[64] = {0};
    const char *diagnostic = "widget properties exceed the lowering size limit";
    int field_count = *kir_skip_ws(source->args) ?
        kir_split_top(source->args, fields[0], 64, sizeof(fields[0])) : 0;
    if(field_count == 64)
        goto failed;
    for(int parameter = 1; parameter < parameter_count; parameter++) {
        char *colon = strchr(parameters[parameter], ':');
        diagnostic = "widget parameters after props must be typed slots";
        if(colon == NULL)
            goto failed;
        *colon++ = '\0';
        kir_trim_in_place(parameters[parameter]);
        kir_trim_in_place(colon);
        const KirType *slot = KirFindType(owner, colon, NULL);
        if(slot == NULL || !slot->is_slot)
            goto failed;
        diagnostic = "widget slot type is shadowed or not directly imported";
        if(KirFindType(c->module, colon, NULL) != slot)
            goto failed;
        slot_types[parameter] = colon;
        int length = snprintf(slot_names[parameter], sizeof(slot_names[parameter]),
                              "%s_slot_%d", temporary, parameter);
        diagnostic = "widget slot binding exceeds the lowering size limit";
        if(length < 0 || (size_t)length >= sizeof(slot_names[parameter]))
            goto failed;
        diagnostic = "widget slot name conflicts with a props field or another slot";
        size_t offset = 0;
        KirTypeField field;
        while(KirTypeNextField(props, &offset, &field) == 1)
            if(!strcmp(field.name, parameters[parameter]))
                goto failed;
        for(int previous = 1; previous < parameter; previous++)
            if(!strcmp(parameters[previous], parameters[parameter]))
                goto failed;
    }
    diagnostic = "widget properties exceed the lowering size limit";
    if(widget_statement(&lowered, KIR_STMT_DECL, span, "%s: %s", temporary, props->name) == NULL)
        goto failed;
    for(int property = 0; property < field_count; property++) {
        char *field = fields[property];
        if(!*field)
            continue;
        char *equals = strchr(field, '=');
        diagnostic = "invalid widget property";
        if(field[0] != '.' || equals == NULL)
            goto failed;
        *equals++ = '\0';
        kir_trim_in_place(field);
        const char *name = field + 1;
        for(int previous = 0; previous < property; previous++) {
            diagnostic = "duplicate widget property or slot";
            if(!strcmp(fields[previous], field))
                goto failed;
        }
        int parameter = 1;
        while(parameter < parameter_count && strcmp(parameters[parameter], name))
            parameter++;
        diagnostic = "widget properties exceed the lowering size limit";
        if(parameter < parameter_count) {
            supplied[parameter] = 1;
            if(widget_statement(&lowered, KIR_STMT_DECL, span, "%s: %s = %s",
                                slot_names[parameter], slot_types[parameter],
                                equals) == NULL)
                goto failed;
        } else {
            char prefix[KIR_NAME_MAX + 3] = "";
            size_t offset = 0;
            KirTypeField field_type;
            int found = 0;
            while(KirTypeNextField(props, &offset, &field_type) == 1) {
                if(!strcmp(field_type.name, name)) {
                    found = 1;
                    if(*kir_skip_ws(equals) == '{')
                        snprintf(prefix, sizeof(prefix), "(%s)", field_type.type);
                    break;
                }
            }
            diagnostic = "unknown widget property or slot";
            if(!found)
                goto failed;
            diagnostic = "widget properties exceed the lowering size limit";
            if(widget_statement(&lowered, KIR_STMT_ASSIGN, span,
                                "%s.%s = %s%s", temporary, name, prefix,
                                kir_skip_ws(equals)) == NULL)
                goto failed;
        }
    }
    char arguments[KIR_TEXT_MAX];
    size_t length = (size_t)snprintf(arguments, sizeof(arguments), "%s", temporary);
    for(int parameter = 1; parameter < parameter_count; parameter++) {
        diagnostic = "missing required widget slot";
        if(!supplied[parameter])
            goto failed;
        int added = snprintf(arguments + length, sizeof(arguments) - length, ", %s", slot_names[parameter]);
        diagnostic = "widget arguments exceed the lowering size limit";
        if(added < 0 || (size_t)added >= sizeof(arguments) - length)
            goto failed;
        length += (size_t)added;
    }
    {
        KirStmt *call = widget_statement(&lowered, KIR_STMT_EXPR, span,
                                         "%s(%s)", source->widget, arguments);
        if(call == NULL)
            goto failed;
        inherit_widget_block_metadata(call, source);
    }
    int replacement_count = lowered.stmt_count;
    int total = c->fn->stmt_count - 1 + replacement_count;
    KirStmt *statements = realloc(c->fn->stmts, (size_t)total * sizeof(*statements));
    if(statements == NULL)
        goto failed;
    memmove(&statements[index + replacement_count], &statements[index + 1],
            (size_t)(c->fn->stmt_count - index - 1) * sizeof(*statements));
    memcpy(&statements[index], lowered.stmts, (size_t)replacement_count * sizeof(*statements));
    c->fn->stmts = statements;
    c->fn->stmt_count = c->fn->stmt_cap = total;
    free(lowered.stmts);
    return replacement_count;
failed:
    KirDiagnostic(span, "check.widget", "%s: %s", diagnostic, source->widget);
    free(lowered.stmts);
    c->failed = 1;
    return -1;
}

static int
resolve_widget_blocks(Checker *c)
{
    int changed = 0;
    for(int i = 0; i < c->fn->stmt_count; i++) {
        KirStmt *statement = &c->fn->stmts[i];
        if(statement->kind != KIR_STMT_WIDGET)
            continue;
        const KirModule *owner = NULL;
        const KirFunction *declaration = NULL;
        int resolved = KirResolveFunction(c->module, statement->widget, &owner, &declaration);
        if(!statement->declared_widget) {
            if(resolved == 1) {
                statement->kind = KIR_STMT_EXPR;
                changed = 1;
            }
            continue;
        }
        if(resolved == 0 && statement->widget_fallback) {
            statement->declared_widget = 0;
            statement->widget_fallback = 0;
            continue;
        }
        char parameters[64][KIR_TEXT_MAX];
        int count = declaration ? kir_split_top(declaration->args, parameters[0], 64, sizeof(parameters[0])) : 0;
        char *type = count > 0 ? strchr(parameters[0], ':') : NULL;
        if(type != NULL) {
            type++;
            kir_trim_in_place(type);
        }
        const KirType *props = type ? KirFindType(owner, type, NULL) : NULL;
        const char *diagnostic = NULL;
        if(resolved < 0)
            diagnostic = "ambiguous widget declaration";
        else if(resolved == 0)
            diagnostic = "unknown widget declaration";
        else if(!declaration->is_ui || declaration->is_extern)
            diagnostic = "widget block requires a #ui declaration";
        else if(count < 1 || count == 64 || props == NULL || props->is_enum || props->is_slot)
            diagnostic = "widget declaration requires one typed record parameter";
        if(diagnostic != NULL) {
            KirDiagnostic(statement->span, "check.widget", "%s: %s", diagnostic, statement->widget);
            c->failed = 1;
            return 0;
        }
        if(KirFindType(c->module, type, NULL) != props) {
            KirDiagnostic(statement->span, "check.widget_type",
                          "widget props type is shadowed or not directly imported: %s", type);
            c->failed = 1;
            return 0;
        }
        if(statement->widget_fallback) {
            /* Host props were provisional. Reuse only the block's field
             * values; the resolved declaration owns the actual record type. */
            char *begin = strchr(statement->args, '{');
            char *end = strrchr(statement->args, '}');
            if(begin == NULL || end == NULL || end <= begin) {
                KirDiagnostic(statement->span, "check.widget", "invalid leaf widget properties: %s",
                              statement->widget);
                c->failed = 1;
                return 0;
            }
            size_t length = (size_t)(end - begin - 1);
            memmove(statement->args, begin + 1, length);
            statement->args[length] = '\0';
            statement->widget_fallback = 0;
        }
        char temporary[KIR_NAME_MAX];
        int serial = i;
        int collision;
        do {
            snprintf(temporary, sizeof(temporary), "widget_value_%d", serial++);
            collision = strstr(c->fn->args, temporary) != NULL;
            for(int s = 0; s < c->fn->stmt_count; s++) {
                collision |= strstr(c->fn->stmts[s].text, temporary) != NULL ||
                             strstr(c->fn->stmts[s].args, temporary) != NULL;
            }
        } while(collision);
        int added = lower_widget_block(c, i, owner, props, temporary, parameters, count);
        if(added < 0)
            return 0;
        i += added - 1;
        changed = 1;
    }
    if(changed)
        KirStructureFunction(c->fn, c->module);
    return 1;
}

static int
return_block_end(const KirFunction *fn, int begin, int end)
{
    int depth = 1;
    for(int i = begin + 1; i < end; i++) {
        KirStmtKind kind = fn->stmts[i].kind;
        if(kind == KIR_STMT_IF || kind == KIR_STMT_WHILE || kind == KIR_STMT_BLOCK_OPEN)
            depth++;
        if(kind == KIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return end;
}

static int sequence_returns(const KirFunction *fn, int begin, int end);

static int
branch_returns(const KirFunction *fn, int begin, int end, int *last)
{
    *last = return_block_end(fn, begin, end);
    int returns = sequence_returns(fn, begin + 1, *last);
    if(fn->stmts[begin].expr_root < 0)
        return returns;
    int next = *last + 1;
    if(next < end && fn->stmts[next].kind == KIR_STMT_IF &&
       !strncmp(fn->stmts[next].text, "else", 4)) {
        int alternative = branch_returns(fn, next, end, last);
        return returns && alternative;
    }
    return 0;
}

static int
sequence_returns(const KirFunction *fn, int begin, int end)
{
    for(int i = begin; i < end; i++) {
        KirStmtKind kind = fn->stmts[i].kind;
        if(kind == KIR_STMT_RETURN)
            return 1;
        if(kind == KIR_STMT_BREAK || kind == KIR_STMT_CONTINUE)
            return 0;
        if(kind == KIR_STMT_IF) {
            int last;
            if(branch_returns(fn, i, end, &last))
                return 1;
            i = last;
        } else if(kind == KIR_STMT_WHILE || kind == KIR_STMT_BLOCK_OPEN) {
            int last = return_block_end(fn, i, end);
            if(kind == KIR_STMT_BLOCK_OPEN && sequence_returns(fn, i + 1, last))
                return 1;
            i = last;
        }
    }
    return 0;
}

static int
check_function(Checker *c, KirFunction *fn)
{
    int strict = c->strict;
    int errors_before = c->errors;
    int has_slots = fn->is_closure;
    int has_arrays = fn->return_type[0] == '[';
    char params[64][KIR_TEXT_MAX];
    int n;
    c->fn = fn; c->count = 0; c->depth = 0;
    c->fn->uses_host = c->fn->is_extern && c->fn->extern_kind == KIR_EXTERN_HOST;
    const KirType *return_slot = KirFindType(c->module, c->fn->return_type, NULL);
    if(return_slot != NULL && return_slot->is_slot) {
        KirDiagnostic(c->fn->span, "check.slot_escape", "slot values cannot escape through returns");
        return 0;
    }
    /* Imports are linked now. Rebuild expressions so imported types
     * participate in cast/grouping decisions before type checking. */
    KirStructureFunction(c->fn, c->module);
    if(!resolve_widget_blocks(c)) {
        return 0;
    }
    n = *kir_skip_ws(c->fn->args) ? kir_split_top(c->fn->args, params[0], 64, sizeof(params[0])) : 0;
    for(int a = 0; a < n; a++) {
        char *colon = strchr(params[a], ':');
        if(colon) {
            *colon++ = 0; kir_trim_in_place(params[a]); kir_trim_in_place(colon);
            const KirType *parameter_type = KirFindType(c->module, colon, NULL);
            has_slots |= parameter_type != NULL && parameter_type->is_slot;
            has_arrays |= KirArrayValueType(colon) || KirSliceElementType(colon, NULL, 0);
            bind(c, params[a], colon, c->fn->span);
        } else error(c, c->fn->span, "strict parameters require name: type", params[a]);
    }
    for(int i = 0; i < c->fn->stmt_count; i++) {
        KirStmt *st = &c->fn->stmts[i];
        const char *type;
        if(st->kind == KIR_STMT_BLOCK_CLOSE) {
            while(c->count && c->bindings[c->count - 1].depth == c->depth) c->count--;
            if(c->depth) c->depth--;
        }
        int errors_before_expression = c->errors;
        if(st->kind == KIR_STMT_DECL)
            normalize_array(c->module, st->type, sizeof(st->type));
        if(st->declared_widget || st->is_instance)
            c->strict = 1;
        if(st->kind == KIR_STMT_DECL && !st->is_instance)
            contextual_slot(c, st->expr_root, st->type);
        if(st->kind == KIR_STMT_ASSIGN && st->lhs_root >= 0 &&
           c->fn->exprs[st->lhs_root].kind == KIR_EXPR_IDENT)
            contextual_slot(c, st->expr_root, lookup(c, c->fn->exprs[st->lhs_root].name));
        type = expression_type(c, st->expr_root);
        if(st->kind == KIR_STMT_WIDGET && st->expr_root >= 0 &&
           *c->fn->exprs[st->expr_root].slot_type)
            st->kind = KIR_STMT_EXPR;
        if(st->kind == KIR_STMT_DECL) {
            if(st->is_instance) {
                const KirType *record = KirFindType(c->module, st->type, NULL);
                if(record == NULL || record->is_enum || record->is_slot)
                    error(c, st->span, "instance state requires a declared record type", st->type);
                const char *key_type = KirScalarType(type);
                if(st->expr_root < 0 || (strcmp(type, "integer") &&
                   key_type[0] != 'i' && key_type[0] != 'u'))
                    error(c, st->span, "instance key requires an integer", st->name);
            } else if(!*st->type) kir_copy(st->type, sizeof(st->type),
                !strcmp(type, "integer") ? "int" : !strcmp(type, "real") ? "double" : type);
            else if(!compatible(st->type, type)) error(c, st->span, "initializer type mismatch", st->name);
            if(st->type[0] == '[') {
                const char *problem = local_storage_error(c->module, st->type);
                if(problem != NULL)
                    error(c, st->span, problem, st->name);
            }
            const KirType *local_type = KirFindType(c->module, st->type, NULL);
            if(local_type != NULL && local_type->is_slot) {
                has_slots = 1;
                if(st->expr_root < 0) {
                    KirDiagnostic(st->span, "check.slot_initializer", "slot bindings require an initializer");
                    c->failed = 1;
                }
            }
            if(!st->is_instance)
                check_borrowed_string(c, st->type, st->expr_root);
            bind(c, st->name, st->type, st->span);
            if(c->count && !strcmp(c->bindings[c->count - 1].name, st->name))
                c->bindings[c->count - 1].is_instance = st->is_instance;
        } else if(st->kind == KIR_STMT_ASSIGN) {
            const char *lhs = expression_type(c, st->lhs_root);
            const KirType *destination = KirFindType(c->module, lhs, NULL);
            if(lhs[0] == '[' && strcmp(st->assignment_op, "="))
                error(c, st->span, "array compound assignment is not supported", st->assignment_op);
            if(destination != NULL && destination->is_slot) {
                int local = c->count - 1;
                const char *name = c->fn->exprs[st->lhs_root].name;
                while(local >= 0 && strcmp(c->bindings[local].name, name))
                    local--;
                if(local < 0 || c->bindings[local].depth != c->depth) {
                    KirDiagnostic(st->span, "check.slot_escape",
                                  "slot assignment cannot escape its lexical block: %s", name);
                    c->failed = 1;
                }
            }
            if(destination != NULL && destination->is_enum && strcmp(st->assignment_op, "="))
                error(c, st->span, "enum compound assignment requires an explicit numeric cast", st->assignment_op);
            if(text_type(lhs) && strcmp(st->assignment_op, "="))
                error(c, st->span, "string compound assignment is not supported", st->assignment_op);
            if(!assignable(c, st->lhs_root)) error(c, st->span, "assignment requires an assignable destination", "");
            if(readonly_text_destination(c->fn, st->lhs_root))
                error(c, st->span, "string bytes and length are read-only", "");
            if(!compatible(lhs, type)) error(c, st->span, "assignment type mismatch", st->text);
            check_borrowed_string(c, lhs, st->expr_root);
        } else if(st->kind == KIR_STMT_RETURN) {
            if(!compatible(c->fn->return_type, type)) error(c, st->span, "return type mismatch", c->fn->name);
            check_borrowed_string(c, c->fn->return_type, st->expr_root);
            if((st->expr_root < 0) != !strcmp(c->fn->return_type, "void"))
                error(c, st->span, "return value does not match function signature", c->fn->name);
        } else if(st->kind == KIR_STMT_IF || st->kind == KIR_STMT_WHILE) {
            if(*type && strcmp(type, "bool")) error(c, st->span, "condition requires bool", type);
        } else if(st->kind == KIR_STMT_RAW || st->kind == KIR_STMT_UNKNOWN ||
                  st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_GOTO ||
                  st->kind == KIR_STMT_LABEL || st->kind == KIR_STMT_WIDGET) {
            if(!module_uses_c(c))
                error(c, st->span, "statement is not supported by strict checking", st->text);
        }
        if(st->kind == KIR_STMT_BLOCK_OPEN || st->kind == KIR_STMT_IF ||
           st->kind == KIR_STMT_WHILE || st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_SWITCH)
            c->depth++;
        c->strict = strict;
        if((st->declared_widget || st->is_instance) && c->errors != errors_before_expression)
            c->failed = 1;
    }
    for(int i = 0; i < fn->expr_count; i++) {
        const KirExpr *call = &fn->exprs[i];
        has_arrays |= KirSliceElementType(call->type, NULL, 0);
        if(call->kind != KIR_EXPR_CALL)
            continue;
        const KirFunction *callee = NULL;
        const KirModule *owner = NULL;
        if(KirResolveFunction(c->module, call->name, &owner, &callee) <= 0 ||
           callee == NULL || callee->is_extern)
            continue;
        has_arrays |= KirArrayValueType(callee->return_type);
        char parameters[64][KIR_TEXT_MAX];
        int count = *kir_skip_ws(callee->args) ?
            kir_split_top(callee->args, parameters[0], 64, sizeof(parameters[0])) : 0;
        for(int parameter = 0; parameter < count; parameter++) {
            const char *colon = strchr(parameters[parameter], ':');
            if(colon != NULL)
                has_arrays |= KirArrayValueType(kir_skip_ws(colon + 1));
        }
    }
    if(!fn->is_extern && fn->return_type[0] == '[' &&
       !sequence_returns(fn, 0, fn->stmt_count))
        error(c, fn->span, "array or slice result requires a return on every path", fn->name);
    c->fn->checked = c->errors == errors_before;
    for(int expression = 0; expression < c->fn->expr_count; expression++)
        has_slots |= c->fn->exprs[expression].is_function_value;
    int has_instances = 0;
    for(int i = 0; i < c->fn->stmt_count; i++)
        has_instances |= c->fn->stmts[i].is_instance;
    c->fn->uses_host |= has_instances;
    if(has_instances && (!c->fn->checked || !KirCanEmitBody(c->module, c->fn))) {
        KirDiagnostic(c->fn->span, "check.instance_body",
                      "instance state requires a fully checked portable body: %s", c->fn->name);
        c->failed = 1;
    }
    if(has_slots && !c->fn->is_extern &&
       (!c->fn->checked || !KirCanEmitBody(c->module, c->fn))) {
        KirDiagnostic(c->fn->span, "check.slot_body",
                      "slot parameters require a fully checked portable body: %s", c->fn->name);
        c->failed = 1;
    }
    if(has_arrays && !fn->is_extern &&
       (!fn->checked || !KirCanEmitBody(c->module, fn))) {
        KirDiagnostic(fn->span, "check.array_body",
                      "array and slice values require a fully checked portable body: %s", fn->name);
        c->failed = 1;
    }
    if(strict && c->fn->checked && !c->fn->is_extern && !KirCanEmitBody(c->module, c->fn)) {
        error(c,c->fn->span,"function is not supported by portable scalar emission",c->fn->name);
        c->fn->checked=0;
    }
    return !c->failed;
}

/* Resolve every declaration before checking bodies, so imported and forward
 * calls compare the same array shapes regardless of traversal order. */
static int
normalize_function_arrays(const KirModule *module, KirFunction *fn)
{
    if(fn->return_type[0] != '[' && strchr(fn->args, '[') == NULL)
        return 1;
    char parts[64][KIR_TEXT_MAX];
    char arguments[sizeof(fn->args)];
    size_t used = 0;
    int count = *kir_skip_ws(fn->args) ?
        kir_split_top(fn->args, parts[0], 64, sizeof(parts[0])) : 0;
    arguments[0] = '\0';
    for(int i = -1; i < count; i++) {
        char *type = fn->return_type;
        size_t capacity = sizeof(fn->return_type);
        if(i >= 0) {
            char *colon = strchr(parts[i], ':');
            if(colon == NULL) {
                KirDiagnostic(fn->span, "check.signature", "parameters require name: type: %s", parts[i]);
                return 0;
            }
            type = colon + 1;
            kir_trim_in_place(type);
            capacity = sizeof(parts[i]) - (size_t)(type - parts[i]);
        }
        int host_buffer = i >= 0 && KirArrayElementType(type, NULL, 0, NULL) &&
                          !KirArrayValueType(type);
        if(type[0] == '[' && !host_buffer) {
            const char *problem = local_storage_error(module, type);
            if(problem == NULL && (fn->is_extern || fn->is_closure))
                problem = "direct array signatures require an ordinary Kry function";
            int bound = -1;
            if(problem == NULL && !KirSliceElementType(type, NULL, 0) &&
               array_capacity(module, type, &bound) != 1)
                problem = "array signatures require a resolved capacity";
            if(problem != NULL) {
                KirDiagnostic(fn->span, "check.array_signature", "%s: %s", problem, type);
                return 0;
            }
            normalize_array(module, type, capacity);
        }
        if(i >= 0) {
            int length = snprintf(arguments + used, sizeof(arguments) - used,
                                  "%s%s", used ? ", " : "", parts[i]);
            if(length < 0 || (size_t)length >= sizeof(arguments) - used) {
                KirDiagnostic(fn->span, "check.array_signature", "function signature exceeds size limit");
                return 0;
            }
            used += (size_t)length;
        }
    }
    kir_copy(fn->args, sizeof(fn->args), arguments);
    return 1;
}

int
KirCheckPrograms(KirProgram **programs, int count, int strict)
{
    Checker c = {0};
    c.programs = programs; c.program_count = count; c.strict = strict;
    /* Link only explicitly imported modules that are present in this build.
     * Host headers remain unresolved; they are not a global type namespace. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            KirModule *module = &programs[p]->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                KirImport *import = &module->imports[i];
                import->resolved_module = NULL;
                if(import->kind == KIR_IMPORT_EXTERN &&
                   (strstr(import->args, "[]") != NULL ||
                    KirSliceElementType(import->return_type, NULL, 0))) {
                    KirDiagnostic(import->span, "check.slice_signature",
                                  "slice signatures require an ordinary Kry function");
                    return 0;
                }
                if(import->kind != KIR_IMPORT_HEADER || strchr(import->target, '.') != NULL)
                    continue;
                for(int q = 0; q < count; q++) {
                    for(int n = 0; n < programs[q]->module_count; n++) {
                        const KirModule *candidate = &programs[q]->modules[n];
                        char stem[KIR_PATH_MAX];
                        size_t length;
                        kir_copy(stem, sizeof(stem), candidate->source_path);
                        length = strlen(stem);
                        if(length > 4 && strcmp(stem + length - 4, ".kry") == 0)
                            stem[length - 4] = '\0';
                        if(strcmp(import->target, candidate->name) != 0 &&
                           strcmp(import->target, stem) != 0)
                            continue;
                        if(import->resolved_module && import->resolved_module != candidate) {
                            KirDiagnostic(import->span, "check.import", "ambiguous Kry import: %s",
                                          import->target);
                            return 0;
                        }
                        import->resolved_module = candidate;
                    }
                }
            }
        }
    }
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            KirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                if(!normalize_function_arrays(module, &module->functions[f]))
                    return 0;
            }
        }
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            if(!check_type_declarations(&programs[p]->modules[m], strict))
                return 0;
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        c.module = &programs[p]->modules[m];
        for(int i = 0; i < c.module->global_count + c.module->state_count; i++) {
            const char *type = i < c.module->global_count ? c.module->globals[i].type :
                c.module->state_fields[i - c.module->global_count].type;
            const KirType *slot = KirFindType(c.module, type, NULL);
            if(strict || strstr(type, "[]") != NULL) {
                ValidatedRecords checked = {0};
                const char *error = storage_type_error(c.module, type, NULL, 0, &checked);
                free(checked.items);
                if(error != NULL && (slot == NULL || !slot->is_slot)) {
                    KirSourceSpan span = i < c.module->global_count ? c.module->globals[i].span :
                        c.module->state_fields[i - c.module->global_count].span;
                    KirDiagnostic(span, "check.storage", "%s: %s", error, type);
                    free(c.bindings);
                    return 0;
                }
            }
            if(slot != NULL && slot->is_slot) {
                KirSourceSpan span = i < c.module->global_count ? c.module->globals[i].span :
                    c.module->state_fields[i - c.module->global_count].span;
                KirDiagnostic(span, "check.slot_escape", "slot values cannot be stored in globals or state");
                free(c.bindings);
                return 0;
            }
        }
        for(int f = 0; f < c.module->function_count; f++) {
            if(c.module->functions[f].is_closure)
                continue;
            if(!check_function(&c, &c.module->functions[f])) {
                free(c.bindings);
                return 0;
            }

        }
    }
    /* Runtime implementations become host methods when they need host services.
     * Propagate through resolved calls, including mutually recursive modules. */
    int changed;
    do {
        changed = 0;
        for(int p = 0; p < count; p++) {
            for(int m = 0; m < programs[p]->module_count; m++) {
                KirModule *module = &programs[p]->modules[m];
                for(int f = 0; f < module->function_count; f++) {
                    KirFunction *fn = &module->functions[f];
                    if(fn->uses_host)
                        continue;
                    for(int x = 0; x < fn->expr_count; x++) {
                        const KirFunction *callee = NULL;
                        const KirModule *owner = NULL;
                        if((fn->exprs[x].kind == KIR_EXPR_CALL || fn->exprs[x].is_function_value) &&
                           KirResolveFunction(module, fn->exprs[x].name, &owner, &callee) == 1 &&
                           callee->uses_host) {
                            fn->uses_host = 1;
                            changed = 1;
                            break;
                        }
                    }
                }
            }
        }
    } while(changed);
    free(c.bindings);
    return !c.failed && (!strict || c.errors == 0) && KirCheckSliceLifetimes(programs, count);
}
