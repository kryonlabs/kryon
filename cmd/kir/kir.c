#include "kir.h"

#include <stdlib.h>
#include <string.h>

static void *
kir_realloc_array(void *ptr, int *cap, int count, size_t elem_size)
{
    void *next;
    int ncap;

    if(count < *cap)
        return ptr;
    ncap = *cap == 0 ? 4 : *cap * 2;
    next = realloc(ptr, (size_t)ncap * elem_size);
    if(next == NULL)
        return NULL;
    memset((char *)next + (size_t)(*cap) * elem_size, 0,
           (size_t)(ncap - *cap) * elem_size);
    *cap = ncap;
    return next;
}

void
kir_copy(char *dst, size_t dst_size, const char *src)
{
    if(dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

KirProgram *
KirProgramNew(void)
{
    return calloc(1, sizeof(KirProgram));
}

void
KirProgramFree(KirProgram *program)
{
    int i;

    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++) {
        KirModule *m = &program->modules[i];
        int j;

        for(j = 0; j < m->function_count; j++) {
            free(m->functions[j].stmts);
            free(m->functions[j].exprs);
        }
        free(m->state_fields);
        free(m->globals);
        free(m->imports);
        free(m->functions);
        free(m->defines);
        free(m->asserts);
        free(m->types);
        free(m->routes);
    }
    free(program->modules);
    free(program);
}

KirSourceSpan
KirSpan(const char *path, int line, int column)
{
    KirSourceSpan span;

    memset(&span, 0, sizeof(span));
    kir_copy(span.path, sizeof(span.path), path);
    span.line = line;
    span.column = column;
    return span;
}

KirModule *
KirProgramAddModule(KirProgram *program, const char *name,
                    const char *source_path, KirSourceSpan span)
{
    KirModule *modules;
    KirModule *m;

    if(program == NULL)
        return NULL;
    modules = kir_realloc_array(program->modules, &program->module_cap,
                                program->module_count, sizeof(KirModule));
    if(modules == NULL)
        return NULL;
    program->modules = modules;
    m = &program->modules[program->module_count++];
    memset(m, 0, sizeof(*m));
    kir_copy(m->name, sizeof(m->name), name);
    kir_copy(m->source_path, sizeof(m->source_path), source_path);
    m->span = span;
    return m;
}

KirStateField *
KirModuleAddStateField(KirModule *module, const char *name, const char *type,
                       const char *init, KirSourceSpan span)
{
    KirStateField *fields;
    KirStateField *f;

    if(module == NULL)
        return NULL;
    fields = kir_realloc_array(module->state_fields, &module->state_cap,
                               module->state_count, sizeof(KirStateField));
    if(fields == NULL)
        return NULL;
    module->state_fields = fields;
    f = &module->state_fields[module->state_count++];
    memset(f, 0, sizeof(*f));
    kir_copy(f->name, sizeof(f->name), name);
    kir_copy(f->type, sizeof(f->type), type);
    kir_copy(f->init, sizeof(f->init), init);
    f->span = span;
    return f;
}

KirImport *
KirModuleAddImport(KirModule *module, KirImportKind kind, const char *name,
                   const char *target, const char *signature, int required,
                   KirSourceSpan span)
{
    KirImport *imports;
    KirImport *imp;

    if(module == NULL)
        return NULL;
    imports = kir_realloc_array(module->imports, &module->import_cap,
                                module->import_count, sizeof(KirImport));
    if(imports == NULL)
        return NULL;
    module->imports = imports;
    imp = &module->imports[module->import_count++];
    memset(imp, 0, sizeof(*imp));
    imp->kind = kind;
    kir_copy(imp->name, sizeof(imp->name), name);
    kir_copy(imp->target, sizeof(imp->target), target);
    kir_copy(imp->signature, sizeof(imp->signature), signature);
    imp->required = required;
    imp->span = span;
    return imp;
}

KirFunction *
KirModuleAddFunction(KirModule *module, const char *name, const char *args,
                     const char *return_type, int exported, KirSourceSpan span)
{
    KirFunction *functions;
    KirFunction *fn;

    if(module == NULL)
        return NULL;
    functions = kir_realloc_array(module->functions, &module->function_cap,
                                  module->function_count, sizeof(KirFunction));
    if(functions == NULL)
        return NULL;
    module->functions = functions;
    fn = &module->functions[module->function_count++];
    memset(fn, 0, sizeof(*fn));
    kir_copy(fn->name, sizeof(fn->name), name);
    kir_copy(fn->args, sizeof(fn->args), args);
    kir_copy(fn->return_type, sizeof(fn->return_type), return_type);
    fn->exported = exported;
    fn->span = span;
    return fn;
}

void
KirModuleAddGlobal(KirModule *module, const char *name, const char *type,
                   const char *init, KirSourceSpan span)
{
    KirGlobal *globals;

    if(module == NULL)
        return;
    globals = kir_realloc_array(module->globals, &module->global_cap,
                                module->global_count, sizeof(KirGlobal));
    if(globals == NULL)
        return;
    module->globals = globals;
    memset(&module->globals[module->global_count], 0, sizeof(KirGlobal));
    kir_copy(module->globals[module->global_count].name,
             sizeof(module->globals[0].name), name);
    kir_copy(module->globals[module->global_count].type,
             sizeof(module->globals[0].type), type);
    kir_copy(module->globals[module->global_count].init,
             sizeof(module->globals[0].init), init);
    module->globals[module->global_count].span = span;
    module->global_count++;
}

void
KirModuleAddStatic(KirModule *module, const char *name, const char *type,
                   const char *init, KirSourceSpan span)
{
    KirModuleAddGlobal(module, name, type, init, span);
    if(module != NULL && module->global_count > 0)
        module->globals[module->global_count - 1].is_static = 1;
}

KirDefine *
KirModuleAddDefine(KirModule *module, const char *name, const char *value,
                   KirSourceSpan span)
{
    KirDefine *defines;
    KirDefine *d;

    if(module == NULL)
        return NULL;
    defines = kir_realloc_array(module->defines, &module->define_cap,
                                module->define_count, sizeof(KirDefine));
    if(defines == NULL)
        return NULL;
    module->defines = defines;
    d = &module->defines[module->define_count++];
    memset(d, 0, sizeof(*d));
    kir_copy(d->name, sizeof(d->name), name);
    kir_copy(d->value, sizeof(d->value), value);
    d->span = span;
    return d;
}

KirAssert *
KirModuleAddAssert(KirModule *module, const char *condition,
                   const char *message, KirSourceSpan span)
{
    KirAssert *asserts;
    KirAssert *a;

    if(module == NULL)
        return NULL;
    asserts = kir_realloc_array(module->asserts, &module->assert_cap,
                                module->assert_count, sizeof(KirAssert));
    if(asserts == NULL)
        return NULL;
    module->asserts = asserts;
    a = &module->asserts[module->assert_count++];
    memset(a, 0, sizeof(*a));
    kir_copy(a->condition, sizeof(a->condition), condition);
    kir_copy(a->message, sizeof(a->message), message);
    a->span = span;
    return a;
}

KirType *
KirModuleAddType(KirModule *module, const char *name, KirSourceSpan span)
{
    KirType *types;

    if(module == NULL)
        return NULL;
    types = kir_realloc_array(module->types, &module->type_cap,
                              module->type_count, sizeof(KirType));
    if(types == NULL)
        return NULL;
    module->types = types;
    memset(&module->types[module->type_count], 0, sizeof(KirType));
    kir_copy(module->types[module->type_count].name,
             sizeof(module->types[0].name), name);
    module->types[module->type_count].span = span;
    return &module->types[module->type_count++];
}

KirRoute *
KirModuleAddRoute(KirModule *module, const char *id, KirSourceSpan span)
{
    KirRoute *routes;
    KirRoute *route;

    if(module == NULL)
        return NULL;
    routes = kir_realloc_array(module->routes, &module->route_cap,
                               module->route_count, sizeof(KirRoute));
    if(routes == NULL)
        return NULL;
    module->routes = routes;
    route = &module->routes[module->route_count++];
    memset(route, 0, sizeof(*route));
    kir_copy(route->id, sizeof(route->id), id);
    kir_copy(route->title, sizeof(route->title), id);
    kir_copy(route->group, sizeof(route->group), "Project");
    kir_copy(route->page, sizeof(route->page), id);
    route->span = span;
    return route;
}

KirStmt *
KirFunctionAddStmt(KirFunction *fn, KirStmtKind kind, const char *text,
                   const char *widget, KirSourceSpan span)
{
    KirStmt *stmts;
    KirStmt *st;

    if(fn == NULL)
        return NULL;
    stmts = kir_realloc_array(fn->stmts, &fn->stmt_cap, fn->stmt_count,
                              sizeof(KirStmt));
    if(stmts == NULL)
        return NULL;
    fn->stmts = stmts;
    st = &fn->stmts[fn->stmt_count++];
    memset(st, 0, sizeof(*st));
    st->kind = kind;
    kir_copy(st->text, sizeof(st->text), text);
    kir_copy(st->widget, sizeof(st->widget), widget);
    st->expr_root = -1;
    st->lhs_root = -1;
    st->span = span;
    return st;
}

KirStmt *
KirFunctionAddWidget(KirFunction *fn, const char *widget, const char *args,
                     const char *text, KirSourceSpan span)
{
    KirStmt *st = KirFunctionAddStmt(fn, KIR_STMT_WIDGET, text, widget, span);

    if(st != NULL)
        kir_copy(st->args, sizeof(st->args), args);
    return st;
}

KirExpr *
KirFunctionAddExpr(KirFunction *fn, KirExprKind kind, const char *text,
                   KirSourceSpan span)
{
    KirExpr *exprs;
    KirExpr *expr;

    if(fn == NULL)
        return NULL;
    exprs = kir_realloc_array(fn->exprs, &fn->expr_cap, fn->expr_count,
                              sizeof(KirExpr));
    if(exprs == NULL)
        return NULL;
    fn->exprs = exprs;
    expr = &fn->exprs[fn->expr_count++];
    memset(expr, 0, sizeof(*expr));
    expr->kind = kind;
    expr->left = -1;
    expr->right = -1;
    expr->first_child = -1;
    expr->next_sibling = -1;
    expr->third = -1;
    kir_copy(expr->text, sizeof(expr->text), text);
    expr->span = span;
    return expr;
}

const char *
KirImportKindName(KirImportKind kind)
{
    switch(kind) {
    case KIR_IMPORT_HEADER: return "header";
    case KIR_IMPORT_MODULE: return "module";
    case KIR_IMPORT_EXTERN: return "extern";
    case KIR_IMPORT_INTRINSIC: return "intrinsic";
    case KIR_IMPORT_CAPABILITY: return "capability";
    case KIR_IMPORT_HOST: return "host";
    default: return "unknown";
    }
}

const char *
KirExternKindName(KirExternKind kind)
{
    switch(kind) {
    case KIR_EXTERN_HOST: return "host";
    case KIR_EXTERN_GO: return "go";
    case KIR_EXTERN_C: return "c";
    default: return "none";
    }
}

const char *
KirExprKindName(KirExprKind kind)
{
    switch(kind) {
    case KIR_EXPR_IDENT: return "ident";
    case KIR_EXPR_INT: return "int";
    case KIR_EXPR_FLOAT: return "float";
    case KIR_EXPR_STRING: return "string";
    case KIR_EXPR_CALL: return "call";
    case KIR_EXPR_BINARY: return "binary";
    case KIR_EXPR_UNARY: return "unary";
    case KIR_EXPR_MEMBER: return "member";
    case KIR_EXPR_POINTER_MEMBER: return "pointer_member";
    case KIR_EXPR_INDEX: return "index";
    case KIR_EXPR_CAST: return "cast";
    case KIR_EXPR_COMPOUND: return "compound";
    case KIR_EXPR_SIZEOF: return "sizeof";
    case KIR_EXPR_CHAR: return "char";
    case KIR_EXPR_CONDITIONAL: return "conditional";
    case KIR_EXPR_POSTFIX: return "postfix";
    default: return "unknown";
    }
}

const char *
KirStmtKindName(KirStmtKind kind)
{
    switch(kind) {
    case KIR_STMT_BLOCK_OPEN: return "block_open";
    case KIR_STMT_BLOCK_CLOSE: return "block_close";
    case KIR_STMT_DECL: return "decl";
    case KIR_STMT_ASSIGN: return "assign";
    case KIR_STMT_EXPR: return "expr";
    case KIR_STMT_IF: return "if";
    case KIR_STMT_WHILE: return "while";
    case KIR_STMT_FOR: return "for";
    case KIR_STMT_SWITCH: return "switch";
    case KIR_STMT_CASE: return "case";
    case KIR_STMT_RETURN: return "return";
    case KIR_STMT_BREAK: return "break";
    case KIR_STMT_CONTINUE: return "continue";
    case KIR_STMT_GOTO: return "goto";
    case KIR_STMT_LABEL: return "label";
    case KIR_STMT_DEFER: return "defer";
    case KIR_STMT_UNUSED: return "unused";
    case KIR_STMT_RAW: return "raw";
    case KIR_STMT_WIDGET: return "widget";
    default: return "unknown";
    }
}

static void
kir_dump_span(FILE *out, KirSourceSpan span)
{
    fprintf(out, "%s:%d:%d", span.path, span.line, span.column);
}

static void
kir_dump_expr(const KirFunction *fn, int index, FILE *out, int indent)
{
    const KirExpr *expr;

    if(fn == NULL || index < 0 || index >= fn->expr_count)
        return;
    expr = &fn->exprs[index];
    for(int i = 0; i < indent; i++)
        fputs("  ", out);
    fprintf(out, "expr %s text %s name %s op %s span ",
            KirExprKindName(expr->kind), expr->text, expr->name, expr->op);
    kir_dump_span(out, expr->span);
    if(expr->type[0]) fprintf(out, " type %s", expr->type);
    fprintf(out, "\n");
    if(expr->left >= 0)
        kir_dump_expr(fn, expr->left, out, indent + 1);
    if(expr->right >= 0)
        kir_dump_expr(fn, expr->right, out, indent + 1);
    if(expr->third >= 0)
        kir_dump_expr(fn, expr->third, out, indent + 1);
    for(int child = expr->first_child; child >= 0 &&
         child < fn->expr_count; child = fn->exprs[child].next_sibling)
        kir_dump_expr(fn, child, out, indent + 1);
}

void
KirProgramDump(const KirProgram *program, FILE *out)
{
    int i;

    if(out == NULL)
        return;
    fprintf(out, "kir 1\n");
    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++) {
        const KirModule *m = &program->modules[i];
        int j;

        fprintf(out, "module %s source %s span ", m->name, m->source_path);
        kir_dump_span(out, m->span);
        fprintf(out, "\n");
        if(m->app.has_app) {
            fprintf(out, "  app title %s size %dx%d fps %d theme %s "
                    "dark %d font_examples %d frame %s init %s scene %s "
                    "shutdown %s\n",
                    m->app.title[0] ? m->app.title : "\"\"",
                    m->app.width, m->app.height, m->app.fps,
                    m->app.theme[0] ? m->app.theme : "",
                    m->app.dark_mode, m->app.font_examples,
                    m->app.frame[0] ? m->app.frame : "",
                    m->app.init[0] ? m->app.init : "",
                    m->app.scene[0] ? m->app.scene : "",
                    m->app.shutdown[0] ? m->app.shutdown : "");
        }
        for(j = 0; j < m->import_count; j++) {
            const KirImport *imp = &m->imports[j];

            fprintf(out, "  import %s %s target %s",
                    KirImportKindName(imp->kind), imp->name, imp->target);
            if(imp->kind == KIR_IMPORT_EXTERN)
                fprintf(out, " extern_kind %s extern_symbol %s",
                        KirExternKindName(imp->extern_kind),
                        imp->extern_symbol);
            fprintf(out, " required %d signature %s span ",
                    imp->required, imp->signature);
            kir_dump_span(out, imp->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->state_count; j++) {
            const KirStateField *f = &m->state_fields[j];

            fprintf(out, "  state %s type %s init %s span ",
                    f->name, f->type, f->init);
            kir_dump_span(out, f->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->route_count; j++) {
            const KirRoute *route = &m->routes[j];

            fprintf(out, "  route %s title %s group %s page %s span ",
                    route->id, route->title, route->group, route->page);
            kir_dump_span(out, route->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->assert_count; j++) {
            const KirAssert *a = &m->asserts[j];

            fprintf(out, "  assert condition %s known %d value %d message %s span ",
                    a->condition, a->known, a->value, a->message);
            kir_dump_span(out, a->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->function_count; j++) {
            const KirFunction *fn = &m->functions[j];
            int k;

            fprintf(out, "  function %s args %s return %s exported %d span ",
                    fn->name, fn->args, fn->return_type, fn->exported);
            kir_dump_span(out, fn->span);
            fprintf(out, "\n");
            for(k = 0; k < fn->stmt_count; k++) {
                const KirStmt *st = &fn->stmts[k];

                fprintf(out, "    stmt %s widget %s args %s text %s span ",
                        KirStmtKindName(st->kind), st->widget, st->args,
                        st->text);
                kir_dump_span(out, st->span);
                fprintf(out, "\n");
                if(st->expr_root >= 0)
                    kir_dump_expr(fn, st->expr_root, out, 3);
                if(st->lhs_root >= 0)
                    kir_dump_expr(fn, st->lhs_root, out, 3);
            }
        }
    }
}
