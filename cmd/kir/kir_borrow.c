#include "kir_borrow.h"
#include "kir_diagnostic.h"
#include "kir_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Origin {
    uint64_t parameters;
    int depth;
    int invalid;
} Origin;

typedef struct BorrowFunction {
    const KirModule *module;
    const KirFunction *fn;
    Origin returned;
    Origin *locals;
} BorrowFunction;

typedef struct BorrowBinding {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    Origin origin;
    int local;
    int depth;
    int captured;
} BorrowBinding;

typedef struct BorrowCheck {
    BorrowFunction *functions;
    int count;
    BorrowFunction *current;
    BorrowBinding *bindings;
    int binding_count;
    int depth;
    int changed;
    int failed;
} BorrowCheck;

static Origin
merge(Origin a, Origin b)
{
    a.parameters |= b.parameters;
    if(b.depth > a.depth)
        a.depth = b.depth;
    a.invalid |= b.invalid;
    return a;
}

static void
accumulate(BorrowCheck *check, Origin *destination, Origin source)
{
    Origin next = merge(*destination, source);
    if(next.parameters != destination->parameters || next.depth != destination->depth ||
       next.invalid != destination->invalid) {
        *destination = next;
        check->changed = 1;
    }
}

static void
reject(BorrowCheck *check, KirSourceSpan span, const char *message)
{
    if(!check->failed)
        KirDiagnostic(span, "check.slice_lifetime", "%s", message);
    check->failed = 1;
}

static BorrowBinding *
binding(BorrowCheck *check, const char *name)
{
    for(int i = check->binding_count - 1; i >= 0; i--)
        if(!strcmp(check->bindings[i].name, name))
            return &check->bindings[i];
    return NULL;
}

static Origin expression_origin(BorrowCheck *check, int index);

static Origin
call_origin(BorrowCheck *check, const KirExpr *expression)
{
    const KirModule *owner = NULL;
    const KirFunction *callee = NULL;
    if(KirResolveFunction(check->current->module, expression->name, &owner, &callee) <= 0)
        return (Origin){0, 0, 1};
    BorrowFunction *summary = NULL;
    for(int i = 0; i < check->count; i++)
        if(check->functions[i].fn == callee)
            summary = &check->functions[i];
    if(summary == NULL)
        return (Origin){0, 0, 1};
    Origin result = {0, summary->returned.depth, summary->returned.invalid};
    int ordinal = 0;
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling, ordinal++) {
        if(ordinal < 64 && (summary->returned.parameters & (UINT64_C(1) << ordinal)))
            result = merge(result, expression_origin(check, child));
    }
    return result;
}

static Origin
expression_origin(BorrowCheck *check, int index)
{
    if(index < 0)
        return (Origin){0};
    const KirFunction *fn = check->current->fn;
    const KirExpr *expression = &fn->exprs[index];
    switch(expression->kind) {
    case KIR_EXPR_IDENT: {
        BorrowBinding *source = binding(check, expression->name);
        if(expression->type[0] != '[' && strchr(expression->type, '*') != NULL)
            return (Origin){0, 0, 1};
        if(source == NULL)
            return (Origin){0}; /* Typed module-owned array or record storage. */
        if(KirSliceElementType(source->type, NULL, 0))
            return source->local >= 0 ? check->current->locals[source->local] : source->origin;
        return (Origin){0, source->depth, source->captured == 2};
    }
    case KIR_EXPR_MEMBER:
    case KIR_EXPR_INDEX:
    case KIR_EXPR_SLICE:
        return expression_origin(check, expression->left);
    case KIR_EXPR_CONDITIONAL:
        return merge(expression_origin(check, expression->right),
                     expression_origin(check, expression->third));
    case KIR_EXPR_CALL:
        if(KirSliceElementType(expression->type, NULL, 0))
            return call_origin(check, expression);
        return (Origin){0, 0, 1}; /* A returned array/record is temporary storage. */
    default:
        return (Origin){0, 0, 1};
    }
}

/* Inspect every range, including ranges passed directly to void/scalar calls. */
static void
check_ranges(BorrowCheck *check, int index)
{
    if(index < 0 || check->failed)
        return;
    const KirExpr *expression = &check->current->fn->exprs[index];
    if(expression->kind == KIR_EXPR_SLICE && expression_origin(check, index).invalid)
        reject(check, expression->span, "slice range requires live owned backing storage");
    check_ranges(check, expression->left);
    check_ranges(check, expression->right);
    check_ranges(check, expression->third);
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling)
        check_ranges(check, child);
}

static void
add_binding(BorrowCheck *check, const char *name, const char *type,
            Origin origin, int local, int depth, int captured)
{
    BorrowBinding *item = &check->bindings[check->binding_count++];
    kir_copy(item->name, sizeof(item->name), name);
    kir_copy(item->type, sizeof(item->type), type);
    item->origin = origin;
    item->local = local;
    item->depth = depth;
    item->captured = captured;
}

static void
check_function(BorrowCheck *check, BorrowFunction *function)
{
    const KirFunction *fn = function->fn;
    check->current = function;
    check->binding_count = 0;
    check->depth = 1;
    check->bindings = calloc((size_t)fn->stmt_count + fn->capture_count + 65, sizeof(*check->bindings));
    if(check->bindings == NULL) {
        reject(check, fn->span, "out of memory checking slice lifetimes");
        return;
    }
    for(int i = 0; i < fn->capture_count; i++) {
        const KirCapture *capture = &fn->captures[i];
        add_binding(check, capture->name, capture->type, (Origin){0}, -1, 0,
                    capture->is_instance ? 2 : 1);
    }
    char parameters[64][KIR_TEXT_MAX];
    int count = *kir_skip_ws(fn->args) ?
        kir_split_top(fn->args, parameters[0], 64, sizeof(parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon == NULL)
            continue;
        *colon++ = '\0';
        kir_trim_in_place(parameters[i]);
        kir_trim_in_place(colon);
        Origin origin = {0};
        int local = -1;
        if(KirSliceElementType(colon, NULL, 0)) {
            origin.parameters = UINT64_C(1) << i;
            local = fn->stmt_count + i;
            accumulate(check, &function->locals[local], origin);
        }
        add_binding(check, parameters[i], colon, origin, local, 1, 0);
    }
    for(int i = 0; i < fn->stmt_count && !check->failed; i++) {
        const KirStmt *statement = &fn->stmts[i];
        if(statement->kind == KIR_STMT_BLOCK_CLOSE) {
            while(check->binding_count && check->bindings[check->binding_count - 1].depth == check->depth)
                check->binding_count--;
            check->depth--;
            continue;
        }
        check_ranges(check, statement->lhs_root);
        check_ranges(check, statement->expr_root);
        if(statement->kind == KIR_STMT_DECL) {
            if(KirSliceElementType(statement->type, NULL, 0)) {
                Origin source = expression_origin(check, statement->expr_root);
                accumulate(check, &function->locals[i], source);
                if(source.invalid || source.depth > check->depth)
                    reject(check, statement->span, "slice initializer outlives its backing storage");
            }
            add_binding(check, statement->name, statement->type, (Origin){0}, i,
                        check->depth, statement->is_instance ? 2 : 0);
        } else if(statement->kind == KIR_STMT_ASSIGN && statement->lhs_root >= 0) {
            const KirExpr *destination = &fn->exprs[statement->lhs_root];
            if(KirSliceElementType(destination->type, NULL, 0)) {
                BorrowBinding *target = destination->kind == KIR_EXPR_IDENT ?
                    binding(check, destination->name) : NULL;
                Origin source = expression_origin(check, statement->expr_root);
                if(target == NULL || target->captured || source.invalid || source.depth > target->depth) {
                    reject(check, statement->span, "slice assignment may escape its backing storage");
                } else if(target->local >= 0) {
                    accumulate(check, &function->locals[target->local], source);
                }
            }
        } else if(statement->kind == KIR_STMT_RETURN && KirSliceElementType(fn->return_type, NULL, 0)) {
            Origin source = expression_origin(check, statement->expr_root);
            if(source.invalid || source.depth > 0)
                reject(check, statement->span, "returned slice borrows local or temporary storage");
            accumulate(check, &function->returned, source);
        }
        if(statement->kind == KIR_STMT_IF || statement->kind == KIR_STMT_WHILE ||
           statement->kind == KIR_STMT_BLOCK_OPEN)
            check->depth++;
    }
    free(check->bindings);
    check->bindings = NULL;
}

int
KirCheckSliceLifetimes(KirProgram **programs, int count)
{
    BorrowCheck check = {0};
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            check.count += programs[p]->modules[m].function_count;
    check.functions = calloc((size_t)check.count, sizeof(*check.functions));
    if(check.functions == NULL && check.count != 0)
        return 0;
    int index = 0;
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            const KirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                BorrowFunction *function = &check.functions[index++];
                function->module = module;
                function->fn = &module->functions[f];
                function->locals = calloc((size_t)function->fn->stmt_count + 64, sizeof(Origin));
                if(function->locals == NULL)
                    check.failed = 1;
            }
        }
    }
    do {
        check.changed = 0;
        for(int i = 0; i < check.count && !check.failed; i++) {
            if(check.functions[i].fn->checked && !check.functions[i].fn->is_extern)
                check_function(&check, &check.functions[i]);
        }
    } while(check.changed && !check.failed);
    for(int i = 0; i < check.count; i++)
        free(check.functions[i].locals);
    free(check.functions);
    return !check.failed;
}
