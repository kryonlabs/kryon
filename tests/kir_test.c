#include "kir.h"
#include "kir_parse.h"
#include "kir_expr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
check(int cond, const char *msg)
{
    if(cond)
        return 1;
    fprintf(stderr, "kir_test: %s\n", msg);
    return 0;
}

static int
check_slice_expressions(void)
{
    const struct {
        const char *source;
        KirExprKind kind;
        int low;
        int high;
    } cases[] = {
        {"items[1:3]", KIR_EXPR_SLICE, 1, 1},
        {"items[:3]", KIR_EXPR_SLICE, 0, 1},
        {"items[1:]", KIR_EXPR_SLICE, 1, 0},
        {"items[:]", KIR_EXPR_SLICE, 0, 0},
        {"items[yes ? 1 : 2:limit()]", KIR_EXPR_SLICE, 1, 1},
        {"items[start():yes ? 3 : 4]", KIR_EXPR_SLICE, 1, 1},
        {"items[1:3][0:1]", KIR_EXPR_SLICE, 1, 1},
        {"items[yes ? 1 : 2]", KIR_EXPR_INDEX, 1, 0},
        {"items[]", KIR_EXPR_UNKNOWN, 0, 0},
        {"items[1:2:3]", KIR_EXPR_UNKNOWN, 0, 0},
        {"items[1:", KIR_EXPR_UNKNOWN, 0, 0},
    };
    int ok = 1;
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        KirFunction expression = {0};
        int root = KirParseExpr(&expression, NULL, cases[i].source,
                                KirSpan("slices.kry", 7, 5));
        ok &= check(root >= 0, cases[i].source);
        if(root >= 0) {
            const KirExpr *node = &expression.exprs[root];
            ok &= check(node->kind == cases[i].kind, cases[i].source);
            if(node->kind != KIR_EXPR_UNKNOWN) {
                ok &= check(node->left >= 0 && (node->right >= 0) == cases[i].low &&
                            (node->third >= 0) == cases[i].high,
                            "range preserves source and omitted bounds");
                ok &= check(node->span.line == 7 && node->span.column == 5,
                            "range preserves source location");
            }
        }
        free(expression.exprs);
    }
    return ok;
}

int
main(void)
{
    KirProgram *program;
    KirModule *module;
    KirFunction *fn;
    char buf[4096];
    size_t n;
    FILE *out;
    int ok = check_slice_expressions();

    char element[KIR_NAME_MAX];
    int capacity;
    ok &= check(KirArrayElementType("[4] i32 ", element, sizeof(element), &capacity) &&
                capacity == 4 && !strcmp(element, "i32"), "normalize fixed array element type");
    ok &= check(!KirArrayElementType("[]i32", element, sizeof(element), &capacity),
                "slice is not a symbolic fixed array");
    ok &= check(!KirArrayElementType("[9999999999999999999999]i32", element, sizeof(element), &capacity),
                "reject overflowing array capacity");
    ok &= check(!KirArrayElementType("[bad-name]i32", element, sizeof(element), &capacity),
                "reject malformed symbolic array capacity");

    ok &= check(KirSliceElementType("[] i32 ", element, sizeof(element)) &&
                !strcmp(element, "i32"), "normalize slice element type");
    ok &= check(!KirSliceElementType("[3]i32", element, sizeof(element)),
                "fixed array is not a slice");
    ok &= check(!KirSliceElementType("[]", element, sizeof(element)) &&
                !KirSliceElementType("[]i32", element, 2),
                "reject missing or truncated slice element");

    /* A module may declare more constants than the initial allocation. */
    {
        char source[8192];
        size_t used = 0;
        int index;
        KirProgram *constants;

        for(index = 0; index < 80; index++) {
            used += (size_t)snprintf(source + used, sizeof(source) - used,
                "VALUE_%d :: %d\n", index, index);
        }
        snprintf(source + used, sizeof(source) - used,
            "#assert VALUE_79 == 79, \"constant table grew\"\n");
        constants = kir_parse_source("constants.kry", source);
        ok &= check(constants != NULL, "grow compile-time constant table");
        if(constants != NULL)
            KirProgramFree(constants);
    }

    KirType record = {0};
    KirTypeField field;
    size_t offset = 0;
    strcpy(record.body, "\n  count : i32\n\tready: bool  \n value: u64");
    ok &= check(KirTypeNextField(&record, &offset, &field) == 1 &&
                !strcmp(field.name, "count") && !strcmp(field.type, "i32"),
                "trim first record field");
    ok &= check(KirTypeNextField(&record, &offset, &field) == 1 &&
                !strcmp(field.name, "ready") && !strcmp(field.type, "bool"),
                "read boolean field");
    ok &= check(KirTypeNextField(&record, &offset, &field) == 1 &&
                !strcmp(field.name, "value") && !strcmp(field.type, "u64"),
                "read field without trailing newline");
    ok &= check(KirTypeNextField(&record, &offset, &field) == 0,
                "record field iterator ends");
    strcpy(record.body, "missing_type:");
    offset = 0;
    ok &= check(KirTypeNextField(&record, &offset, &field) == -1,
                "reject missing record field type");
    strcpy(record.body, "bad name: i32");
    offset = 0;
    ok &= check(KirTypeNextField(&record, &offset, &field) == -1,
                "reject malformed record field name");

    KirModule provider = {0}, other_provider = {0}, consumer = {0};
    KirType other_record = {0};
    KirImport type_imports[2] = {{0}};
    const KirModule *owner = NULL;
    strcpy(record.name, "Value");
    strcpy(other_record.name, "Value");
    provider.types = &record;
    provider.type_count = 1;
    other_provider.types = &other_record;
    other_provider.type_count = 1;
    consumer.imports = type_imports;
    consumer.import_count = 1;
    type_imports[0].kind = KIR_IMPORT_HEADER;
    ok &= check(KirFindType(&consumer, "Value", &owner) == NULL,
                "unresolved imports do not expose types");
    type_imports[0].resolved_module = &provider;
    ok &= check(KirFindType(&consumer, "Value", &owner) == &record && owner == &provider,
                "imported type retains its defining scope");
    type_imports[0].kind = KIR_IMPORT_MODULE;
    ok &= check(KirFindType(&consumer, "Value", &owner) == NULL && owner == NULL,
                "aliased import does not leak unqualified types");
    type_imports[0].kind = KIR_IMPORT_HEADER;
    type_imports[1].kind = KIR_IMPORT_HEADER;
    type_imports[1].resolved_module = &other_provider;
    consumer.import_count = 2;
    ok &= check(KirFindType(&consumer, "Value", &owner) == NULL && owner == NULL,
                "ambiguous imports do not select a type by source order");
    consumer.types = &other_record;
    consumer.type_count = 1;
    ok &= check(KirFindType(&consumer, "Value", &owner) == &other_record && owner == &consumer,
                "local declaration owns its lexical name");

    program = KirProgramNew();
    ok &= check(program != NULL, "new program");
    module = KirProgramAddModule(program, "app", "app.kry",
                                 KirSpan("app.kry", 1, 1));
    ok &= check(module != NULL, "add module");
    ok &= check(KirModuleAddImport(module, KIR_IMPORT_CAPABILITY,
                                   "storage.sqlite", "storage.sqlite",
                                   "(string)->handle", 1,
                                   KirSpan("app.kry", 2, 1)) != NULL,
                "add import");
    ok &= check(KirModuleAddStateField(module, "click_count", "int", "0",
                                       KirSpan("app.kry", 5, 5)) != NULL,
                "add state");
    fn = KirModuleAddFunction(module, "Counter", "state: *CounterState",
                              "void", 1, KirSpan("app.kry", 8, 1));
    ok &= check(fn != NULL, "add function");
    ok &= check(KirFunctionAddWidget(fn, "Text",
                                     "(TextProps){.text=\"Count\"}",
                                     "Text((TextProps){.text=\"Count\"})",
                                     KirSpan("app.kry", 9, 5)) != NULL,
                "add widget stmt");
    ok &= check(KirFunctionAddStmt(fn, KIR_STMT_ASSIGN,
                                   "state->click_count += 1", "",
                                   KirSpan("app.kry", 11, 9)) != NULL,
                "add assign stmt");
    if(!ok) {
        KirProgramFree(program);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    out = tmpfile();
    if(!check(out != NULL, "tmpfile")) {
        KirProgramFree(program);
        return 1;
    }
    KirProgramDump(program, out);
    rewind(out);
    n = fread(buf, 1, sizeof(buf) - 1, out);
    buf[n] = '\0';
    fclose(out);

    ok &= check(strstr(buf, "kir 1\n") != NULL, "dump header");
    ok &= check(strstr(buf, "module app source app.kry span app.kry:1:1") != NULL,
                "dump module");
    ok &= check(strstr(buf, "import capability storage.sqlite") != NULL,
                "dump import");
    ok &= check(strstr(buf, "state click_count type int init 0") != NULL,
                "dump state");
    ok &= check(strstr(buf, "function Counter args state: *CounterState") != NULL,
                "dump function");
    ok &= check(strstr(buf, "stmt widget widget Text args (TextProps){.text=\"Count\"} text Text((TextProps){.text=\"Count\"})") != NULL,
                "dump widget stmt");

    KirProgramFree(program);
    return ok ? 0 : 1;
}
