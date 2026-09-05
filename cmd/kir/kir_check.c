#include "kir_check.h"
#include "kir_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct Binding {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    int depth;
} Binding;

typedef struct Checker {
    KirProgram **programs;
    int program_count;
    KirModule *module;
    KirFunction *fn;
    Binding *bindings;
    int count, capacity, depth, strict, errors;
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
        {"f32", "f32"}, {"f64", "f64"}, {NULL, NULL}
    };
    for(int i = 0; types[i].source; i++)
        if(!strcmp(type, types[i].source)) return types[i].type;
    return "";
}

static void
error(Checker *c, KirSourceSpan span, const char *message, const char *detail)
{
    if(!c->strict) return;
    fprintf(stderr, "%s:%d:%d: %s%s%s\n", span.path, span.line, span.column,
            message, detail && *detail ? ": " : "", detail ? detail : "");
    c->errors++;
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
        if(!next) { c->errors++; return; }
        c->bindings = next; c->capacity = size;
    }
    kir_copy(c->bindings[c->count].name, KIR_NAME_MAX, name);
    kir_copy(c->bindings[c->count].type, KIR_NAME_MAX, type);
    c->bindings[c->count++].depth = c->depth;
}

static const KirFunction *
function(Checker *c, const char *name)
{
    for(int f = 0; f < c->module->function_count; f++)
        if(!strcmp(c->module->functions[f].name, name)) return &c->module->functions[f];
    for(int p = 0; p < c->program_count; p++)
        for(int m = 0; m < c->programs[p]->module_count; m++) {
            const KirModule *module = &c->programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++)
                if(!strcmp(module->functions[f].name, name)) return &module->functions[f];
        }
    return NULL;
}

static const char *
lookup(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!strcmp(c->bindings[i].name, name)) return c->bindings[i].type;
    for(int i = 0; i < c->module->state_count; i++)
        if(!strcmp(c->module->state_fields[i].name, name)) return c->module->state_fields[i].type;
    for(int i = 0; i < c->module->global_count; i++)
        if(!strcmp(c->module->globals[i].name, name)) return c->module->globals[i].type;
    return "";
}

static int
numeric(const char *type)
{
    return !strcmp(type, "integer") || !strcmp(type, "real") ||
           (*type && strchr("iuf", type[0]) && *KirScalarType(type)) || !strcmp(type, "char");
}

static int
compatible(const char *to, const char *from)
{
    const char *canonical = KirScalarType(to);
    if(*canonical) to = canonical;
    if(!*to || !*from) return 1;
    if(!strcmp(to, from)) return 1;
    if(!strcmp(from, "integer") && numeric(to)) return 1;
    if(!strcmp(from, "real") && (to[0] == 'f')) return 1;
    return 0;
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

static const char *
expression_type(Checker *c, int index)
{
    KirExpr *e;
    const char *type = "", *left = "", *right = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->left >= 0) left = expression_type(c, e->left);
    if(e->right >= 0) right = expression_type(c, e->right);
    switch(e->kind) {
    case KIR_EXPR_INT: type = "integer"; break;
    case KIR_EXPR_FLOAT: type = "real"; break;
    case KIR_EXPR_CHAR: type = "char"; break;
    case KIR_EXPR_STRING: type = "const char*"; break;
    case KIR_EXPR_IDENT:
        if(!strcmp(e->name, "true") || !strcmp(e->name, "false")) type = "bool";
        else type = lookup(c, e->name);
        if(!*type) error(c, e->span, "unresolved name", e->name);
        break;
    case KIR_EXPR_CALL: {
        const KirFunction *callee = function(c, e->name);
        const char *args = callee ? callee->args : NULL;
        const char *return_type = callee ? callee->return_type : "";
        char (*parts)[KIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        if(!parts) { c->errors++; break; }
        if(*lookup(c, e->name)) error(c, e->span, "binding is not a callable function", e->name);
        if(!callee) for(int i = 0; i < c->module->import_count; i++) {
            const KirImport *imp = &c->module->imports[i];
            if(imp->kind == KIR_IMPORT_EXTERN && !strcmp(imp->name, e->name)) {
                args = imp->args; return_type = imp->return_type; break;
            }
        }
        expected = args && *kir_skip_ws(args) ? kir_split_top(args, parts[0], 64, sizeof(parts[0])) : 0;
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            const char *arg_type = expression_type(c, child);
            if(args && actual < expected) {
                char *colon = strchr(parts[actual], ':');
                if(colon && !compatible(kir_skip_ws(colon + 1), arg_type))
                    error(c, c->fn->exprs[child].span, "argument type mismatch", e->name);
            }
            actual++;
        }
        if(args) {
            type = return_type;
            if(actual != expected) error(c, e->span, "argument count mismatch", e->name);
        } else error(c, e->span, "unresolved function", e->name);
        free(parts);
        break;
    }
    case KIR_EXPR_BINARY:
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
    case KIR_EXPR_UNARY:
        if((!strcmp(e->op, "++") || !strcmp(e->op, "--")) && !assignable(c, e->right))
            error(c, e->span, "increment requires an assignable expression", "");
        if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else error(c, e->span, "unresolved unary operation", e->op);
        break;
    case KIR_EXPR_POSTFIX:
        if(!assignable(c, e->left)) error(c, e->span, "increment requires an assignable expression", "");
        type = left;
        break;
    case KIR_EXPR_CAST: type = e->name; break;
    case KIR_EXPR_CONDITIONAL: {
        const char *third = expression_type(c, e->third);
        if(strcmp(left, "bool")) error(c, e->span, "conditional requires bool", left);
        if(!compatible(right, third) && !compatible(third, right))
            error(c, e->span, "conditional arms have different types", "");
        type = !strcmp(right, "integer") ? third : right;
        break;
    }
    default: error(c, e->span, "expression is not supported by strict checking", e->text); break;
    }
    if(*KirScalarType(type)) type = KirScalarType(type);
    kir_copy(e->type, sizeof(e->type), type);
    return e->type;
}

int
KirCheckPrograms(KirProgram **programs, int count, int strict)
{
    Checker c = {0};
    c.programs = programs; c.program_count = count; c.strict = strict;
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        c.module = &programs[p]->modules[m];
        for(int f = 0; f < c.module->function_count; f++) {
            char params[64][KIR_TEXT_MAX];
            int n;
            c.fn = &c.module->functions[f]; c.count = 0; c.depth = 0;
            n = *kir_skip_ws(c.fn->args) ? kir_split_top(c.fn->args, params[0], 64, sizeof(params[0])) : 0;
            for(int a = 0; a < n; a++) {
                char *colon = strchr(params[a], ':');
                if(colon) {
                    *colon++ = 0; kir_trim_in_place(params[a]); kir_trim_in_place(colon);
                    bind(&c, params[a], colon, c.fn->span);
                } else error(&c, c.fn->span, "strict parameters require name: type", params[a]);
            }
            for(int i = 0; i < c.fn->stmt_count; i++) {
                KirStmt *st = &c.fn->stmts[i];
                const char *type;
                if(st->kind == KIR_STMT_BLOCK_CLOSE) {
                    while(c.count && c.bindings[c.count - 1].depth == c.depth) c.count--;
                    if(c.depth) c.depth--;
                }
                type = expression_type(&c, st->expr_root);
                if(st->kind == KIR_STMT_DECL) {
                    if(!*st->type) kir_copy(st->type, sizeof(st->type),
                        !strcmp(type, "integer") ? "int" : !strcmp(type, "real") ? "double" : type);
                    else if(!compatible(st->type, type)) error(&c, st->span, "initializer type mismatch", st->name);
                    bind(&c, st->name, st->type, st->span);
                } else if(st->kind == KIR_STMT_ASSIGN) {
                    const char *lhs = expression_type(&c, st->lhs_root);
                    if(!assignable(&c, st->lhs_root)) error(&c, st->span, "assignment requires an assignable destination", "");
                    if(!compatible(lhs, type)) error(&c, st->span, "assignment type mismatch", st->text);
                } else if(st->kind == KIR_STMT_RETURN) {
                    if(!compatible(c.fn->return_type, type)) error(&c, st->span, "return type mismatch", c.fn->name);
                    if((st->expr_root < 0) != !strcmp(c.fn->return_type, "void"))
                        error(&c, st->span, "return value does not match function signature", c.fn->name);
                } else if(st->kind == KIR_STMT_IF || st->kind == KIR_STMT_WHILE) {
                    if(*type && strcmp(type, "bool")) error(&c, st->span, "condition requires bool", type);
                } else if(st->kind == KIR_STMT_RAW || st->kind == KIR_STMT_UNKNOWN ||
                          st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_GOTO ||
                          st->kind == KIR_STMT_LABEL || st->kind == KIR_STMT_WIDGET)
                    error(&c, st->span, "statement is not supported by strict checking", st->text);
                if(st->kind == KIR_STMT_BLOCK_OPEN || st->kind == KIR_STMT_IF ||
                   st->kind == KIR_STMT_WHILE || st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_SWITCH)
                    c.depth++;
            }
        }
    }
    free(c.bindings);
    return c.errors == 0;
}
