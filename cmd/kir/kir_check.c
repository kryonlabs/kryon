#include "kir_check.h"
#include "kir_text.h"
#include "kir_emit.h"
#include "kir_expr.h"

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
    fprintf(stderr, "%s:%d:%d: %s%s%s\n", span.path, span.line, span.column,
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

static const char *
lookup(Checker *c, const char *name)
{
    for(int i = c->count - 1; i >= 0; i--)
        if(!strcmp(c->bindings[i].name, name)) return c->bindings[i].type;
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
compatible(const char *to, const char *from)
{
    const char *canonical = KirScalarType(to);
    if(*canonical) to = canonical;
    if(!*to || !*from) return 1;
    if(!strcmp(to, from)) return 1;
    if(!strcmp(to, "const char*") && !strcmp(from, "string")) return 1;
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
    char member_type[KIR_NAME_MAX] = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->left >= 0) left = expression_type(c, e->left);
    if(e->right >= 0) right = expression_type(c, e->right);
    switch(e->kind) {
    case KIR_EXPR_COMPOUND: {
        const KirType *record = KirFindType(c->module, e->name, NULL);
        int ordinal = 0;
        int mode = -1;
        if(record == NULL || record->is_enum) {
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
            ordinal++;
        }
        type = e->name;
        break;
    }
    case KIR_EXPR_MEMBER: {
        const KirType *record = KirFindType(c->module, left, NULL);
        if(record != NULL && !record->is_enum) {
            size_t offset = 0;
            KirTypeField field;
            while(KirTypeNextField(record, &offset, &field) == 1) {
                if(!strcmp(field.name, e->name)) {
                    kir_copy(member_type, sizeof(member_type), field.type);
                    break;
                }
            }
        }
        if(!*member_type) error(c, e->span, "unknown record field", e->name);
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
        if(!*type) error(c, e->span, "unresolved name", e->name);
        break;
    case KIR_EXPR_CALL: {
        const KirFunction *callee = function(c, e->name, e->span);
        const char *args = callee ? callee->args : NULL;
        const char *return_type = callee ? callee->return_type : "";
        char (*parts)[KIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        if(!parts) { c->errors++; c->failed=1; break; }
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
                if(colon && !c->fn->is_ui && !strcmp(arg_type, "string") &&
                   !strcmp(kir_skip_ws(colon + 1), "const char*"))
                    error(c, c->fn->exprs[child].span,
                          "portable string requires a length-aware foreign parameter", e->name);
                if(colon && !compatible(kir_skip_ws(colon + 1), arg_type))
                    signature_error(c, callee, c->fn->exprs[child].span,
                                    "argument type mismatch", e->name);
                if(colon && (!strcmp(arg_type, "integer") || !strcmp(arg_type, "real"))) {
                    const char *context = KirScalarType(kir_skip_ws(colon + 1));
                    if(*context) kir_copy(c->fn->exprs[child].type, KIR_NAME_MAX, context);
                }
            }
            actual++;
        }
        if(args) {
            type = return_type;
            if(actual != expected)
                signature_error(c, callee, e->span, "argument count mismatch", e->name);
        } else error(c, e->span, "unresolved function", e->name);
        free(parts);
        break;
    }
    case KIR_EXPR_BINARY:
        if((!strcmp(left, "string") || !strcmp(right, "string")) &&
           strcmp(e->op, "==") && strcmp(e->op, "!="))
            error(c, e->span, "string operation is not supported", e->op);
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
        if(!strcmp(e->op, "!") && strcmp(right, "bool"))
            error(c, e->span, "logical operand requires bool", e->op);
        if((!strcmp(e->op, "++") || !strcmp(e->op, "--")) && !assignable(c, e->right))
            error(c, e->span, "increment requires an assignable expression", "");
        if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else error(c, e->span, "unresolved unary operation", e->op);
        break;
    case KIR_EXPR_POSTFIX:
        if(!numeric(left)) error(c, e->span, "increment requires a numeric value", e->op);
        if(!assignable(c, e->left)) error(c, e->span, "increment requires an assignable expression", "");
        type = left;
        break;
    case KIR_EXPR_CAST: {
        const KirType *destination = KirFindType(c->module, e->name, NULL);
        const KirType *source = KirFindType(c->module, right, NULL);
        if(destination != NULL && destination->is_enum &&
           !numeric(right) && strcmp(right, "bool") &&
           (source == NULL || !source->is_enum))
            error(c, e->span, "enum casts require a numeric, bool, or enum value", e->name);
        if((!strcmp(right, "string") || !strcmp(e->name, "string")) && strcmp(right, e->name))
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
        break;
    }
    default: error(c, e->span, "expression is not supported by strict checking", e->text); break;
    }
    if(*KirScalarType(type)) type = KirScalarType(type);
    kir_copy(e->type, sizeof(e->type), type);
    return e->type;
}

static int
record_declaration_error(const KirType *record, const char *message,
                         const char *field)
{
    fprintf(stderr, "%s:%d:%d: %s: %s%s%s\n",
            record->span.path, record->span.line, record->span.column,
            message, record->name, field && *field ? "." : "",
            field ? field : "");
    return 0;
}

/* Structural errors are invalid in every backend, including non-strict mode.
 * Check them before function eligibility can select a fallback emitter. */
static int
check_type_declarations(const KirModule *module)
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
        while((status = KirTypeNextField(record, &offset, &field)) == 1) {
            size_t previous_offset = 0;
            KirTypeField previous;

            if(strcmp(field.type, "void") == 0)
                return record_declaration_error(record, "record field cannot have void type", field.name);
            while(KirTypeNextField(record, &previous_offset, &previous) == 1 &&
                  previous_offset < offset) {
                if(strcmp(previous.name, field.name) == 0)
                    return record_declaration_error(record, "duplicate record field", field.name);
            }
        }
        if(status < 0)
            return record_declaration_error(record, "malformed record field", NULL);
    }
    return 1;
}

static int
resolve_widget_blocks(Checker *c)
{
    int changed = 0;
    for(int i = 0; i < c->fn->stmt_count; i++) {
        KirStmt *statement = &c->fn->stmts[i];
        if(!statement->declared_widget || statement->kind != KIR_STMT_WIDGET)
            continue;
        const KirModule *owner = NULL;
        const KirFunction *declaration = NULL;
        int resolved = KirResolveFunction(c->module, statement->widget, &owner, &declaration);
        if(resolved == 0 && statement->widget_fallback) {
            statement->declared_widget = 0;
            statement->widget_fallback = 0;
            continue;
        }
        char parameters[2][KIR_TEXT_MAX];
        int count = declaration ? kir_split_top(declaration->args, parameters[0], 2, sizeof(parameters[0])) : 0;
        char *type = count == 1 ? strchr(parameters[0], ':') : NULL;
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
        else if(count != 1 || props == NULL || props->is_enum)
            diagnostic = "widget declaration requires one typed record parameter";
        if(diagnostic != NULL) {
            fprintf(stderr, "%s:%d:%d: %s: %s\n", statement->span.path,
                    statement->span.line, statement->span.column, diagnostic, statement->widget);
            c->failed = 1;
            return 0;
        }
        if(KirFindType(c->module, type, NULL) != props) {
            fprintf(stderr, "%s:%d: widget props type is shadowed or not directly imported: %s\n",
                    statement->span.path, statement->span.line, type);
            c->failed = 1;
            return 0;
        }
        if(statement->widget_fallback) {
            /* Host props were provisional. Reuse only the block's field
             * values; the resolved declaration owns the actual record type. */
            char *begin = strchr(statement->args, '{');
            char *end = strrchr(statement->args, '}');
            if(begin == NULL || end == NULL || end <= begin) {
                fprintf(stderr, "%s:%d: invalid leaf widget properties: %s\n",
                        statement->span.path, statement->span.line, statement->widget);
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
        char initializer[KIR_TEXT_MAX];
        int length = snprintf(initializer, sizeof(initializer), "%s: %s = (%s){%s}",
                              temporary, type, type, statement->args);
        if(length < 0 || (size_t)length >= sizeof(initializer)) {
            fprintf(stderr, "%s:%d: widget property exceeds output limit\n",
                    statement->span.path, statement->span.line);
            c->failed = 1;
            return 0;
        }
        /* Props use the ordinary record constructor and its field validation.
         * Evaluate it before the call, including in mixed host-control-flow
         * bodies whose backend still requires a named argument value. */
        KirSourceSpan span = statement->span;
        KirStmt *statements = realloc(c->fn->stmts,
            (size_t)(c->fn->stmt_count + 1) * sizeof(*statements));
        if(statements == NULL) {
            c->failed = 1;
            return 0;
        }
        c->fn->stmts = statements;
        memmove(&statements[i + 1], &statements[i],
                (size_t)(c->fn->stmt_count - i) * sizeof(*statements));
        c->fn->stmt_count++;
        c->fn->stmt_cap = c->fn->stmt_count;
        memset(&statements[i], 0, sizeof(*statements));
        statements[i].span = span;
        statements[i].declared_widget = 1;
        statements[i].kind = KIR_STMT_DECL;
        kir_copy(statements[i].text, sizeof(statements[i].text), initializer);
        statement = &statements[++i];
        /* A block is a statement invocation: it may discard a widget's action
         * result, just like an ordinary call used as a statement. */
        statement->kind = KIR_STMT_EXPR;
        snprintf(statement->text, sizeof(statement->text), "%s(%s)", statement->widget, temporary);
        kir_copy(statement->args, sizeof(statement->args), temporary);
        changed = 1;
    }
    if(changed)
        KirStructureFunction(c->fn, c->module);
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
                            fprintf(stderr, "%s:%d:%d: ambiguous Kry import: %s\n",
                                    import->span.path, import->span.line, import->span.column, import->target);
                            return 0;
                        }
                        import->resolved_module = candidate;
                    }
                }
            }
        }
    }
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        c.module = &programs[p]->modules[m];
        if(!check_type_declarations(c.module)) {
            free(c.bindings);
            return 0;
        }
        for(int f = 0; f < c.module->function_count; f++) {
            int errors_before = c.errors;
            char params[64][KIR_TEXT_MAX];
            int n;
            c.fn = &c.module->functions[f]; c.count = 0; c.depth = 0;
            /* Imports are linked now. Rebuild expressions so imported types
             * participate in cast/grouping decisions before type checking. */
            KirStructureFunction(c.fn, c.module);
            if(!resolve_widget_blocks(&c)) {
                free(c.bindings);
                return 0;
            }
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
                int errors_before_expression = c.errors;
                if(st->declared_widget || st->is_instance)
                    c.strict = 1;
                type = expression_type(&c, st->expr_root);
                if(st->kind == KIR_STMT_DECL) {
                    if(st->is_instance) {
                        const KirType *record = KirFindType(c.module, st->type, NULL);
                        if(record == NULL || record->is_enum)
                            error(&c, st->span, "instance state requires a declared record type", st->type);
                        const char *key_type = KirScalarType(type);
                        if(st->expr_root < 0 || (strcmp(type, "integer") &&
                           key_type[0] != 'i' && key_type[0] != 'u'))
                            error(&c, st->span, "instance key requires an integer", st->name);
                    } else if(!*st->type) kir_copy(st->type, sizeof(st->type),
                        !strcmp(type, "integer") ? "int" : !strcmp(type, "real") ? "double" : type);
                    else if(!compatible(st->type, type)) error(&c, st->span, "initializer type mismatch", st->name);
                    bind(&c, st->name, st->type, st->span);
                } else if(st->kind == KIR_STMT_ASSIGN) {
                    const char *lhs = expression_type(&c, st->lhs_root);
                    const KirType *destination = KirFindType(c.module, lhs, NULL);
                    if(destination != NULL && destination->is_enum && strcmp(st->assignment_op, "="))
                        error(&c, st->span, "enum compound assignment requires an explicit numeric cast", st->assignment_op);
                    if(!strcmp(lhs, "string") && strcmp(st->assignment_op, "="))
                        error(&c, st->span, "string compound assignment is not supported", st->assignment_op);
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
                c.strict = strict;
                if((st->declared_widget || st->is_instance) && c.errors != errors_before_expression)
                    c.failed = 1;
            }
            c.fn->checked = c.errors == errors_before;
            int has_instances = 0;
            for(int i = 0; i < c.fn->stmt_count; i++)
                has_instances |= c.fn->stmts[i].is_instance;
            c.fn->uses_instance_host = has_instances;
            if(has_instances && (!c.fn->checked || !KirCanEmitBody(c.module, c.fn))) {
                fprintf(stderr, "%s:%d: instance state requires a fully checked portable body: %s\n",
                        c.fn->span.path, c.fn->span.line, c.fn->name);
                c.failed = 1;
            }
            if(strict && c.fn->checked && !c.fn->is_extern && !KirCanEmitBody(c.module, c.fn)) {
                error(&c,c.fn->span,"function is not supported by portable scalar emission",c.fn->name);
                c.fn->checked=0;
            }
        }
    }
    /* Runtime implementations become host methods only when they need state.
     * Propagate through resolved calls, including mutually recursive modules. */
    int changed;
    do {
        changed = 0;
        for(int p = 0; p < count; p++) {
            for(int m = 0; m < programs[p]->module_count; m++) {
                KirModule *module = &programs[p]->modules[m];
                for(int f = 0; f < module->function_count; f++) {
                    KirFunction *fn = &module->functions[f];
                    if(fn->uses_instance_host)
                        continue;
                    for(int x = 0; x < fn->expr_count; x++) {
                        const KirFunction *callee = NULL;
                        const KirModule *owner = NULL;
                        if(fn->exprs[x].kind == KIR_EXPR_CALL &&
                           KirResolveFunction(module, fn->exprs[x].name, &owner, &callee) == 1 &&
                           callee->uses_instance_host) {
                            fn->uses_instance_host = 1;
                            changed = 1;
                            break;
                        }
                    }
                }
            }
        }
    } while(changed);
    free(c.bindings);
    return !c.failed && (!strict || c.errors == 0);
}
