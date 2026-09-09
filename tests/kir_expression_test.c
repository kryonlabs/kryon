#include "kir_expr.h"
#include "kir_token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int
parse(KirFunction *fn, const char *text)
{
    return KirParseExpr(fn, NULL, text, KirSpan("expression.kry", 7, 3));
}

int
main(void)
{
    KirFunction fn = {0};
    int e = parse(&fn, "a <= b && c != d");
    assert(fn.exprs[e].kind == KIR_EXPR_BINARY && !strcmp(fn.exprs[e].op, "&&"));
    assert(!strcmp(fn.exprs[fn.exprs[e].left].op, "<="));
    assert(!strcmp(fn.exprs[fn.exprs[e].right].op, "!="));
    e = parse(&fn, "a + b << 2 & mask | flag");
    assert(!strcmp(fn.exprs[e].op, "|"));
    e = fn.exprs[e].left;
    assert(!strcmp(fn.exprs[e].op, "&"));
    e = fn.exprs[e].left;
    assert(!strcmp(fn.exprs[e].op, "<<"));
    assert(!strcmp(fn.exprs[fn.exprs[e].left].op, "+"));
    e = parse(&fn, "10 - 3 - 2");
    assert(!strcmp(fn.exprs[fn.exprs[e].left].op, "-"));
    e = parse(&fn, "x * - y + 1e-3");
    assert(!strcmp(fn.exprs[e].op, "+"));
    assert(fn.exprs[fn.exprs[e].right].kind == KIR_EXPR_FLOAT);
    e = parse(&fn, "a ? b : c ? d : e");
    assert(fn.exprs[e].kind == KIR_EXPR_CONDITIONAL);
    assert(fn.exprs[fn.exprs[e].third].kind == KIR_EXPR_CONDITIONAL);
    e = parse(&fn, "matrix[i][j].value");
    assert(fn.exprs[e].kind == KIR_EXPR_MEMBER);
    e = fn.exprs[e].left;
    assert(fn.exprs[e].kind == KIR_EXPR_INDEX);
    assert(fn.exprs[fn.exprs[e].left].kind == KIR_EXPR_INDEX);
    e = parse(&fn, "callbacks[i](x, 'a')");
    assert(fn.exprs[e].kind == KIR_EXPR_CALL && fn.exprs[e].left >= 0);
    assert(fn.exprs[fn.exprs[fn.exprs[e].first_child].next_sibling].kind == KIR_EXPR_CHAR);
    e = parse(&fn, "(int32_t)(x + 1)");
    assert(fn.exprs[e].kind == KIR_EXPR_CAST && !strcmp(fn.exprs[e].name, "int32_t"));
    {
        KirType phase = {0};
        strcpy(phase.name, "Phase");
        phase.is_enum = 1;
        KirModule provider = {0};
        provider.types = &phase;
        provider.type_count = 1;
        KirImport import = {0};
        import.kind = KIR_IMPORT_HEADER;
        import.resolved_module = &provider;
        KirModule consumer = {0};
        consumer.imports = &import;
        consumer.import_count = 1;
        e = KirParseExpr(&fn, &consumer, "(Phase)value", KirSpan("consumer.kry", 1, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_CAST);
        assert(!strcmp(fn.exprs[e].name, "Phase"));
        assert(!strcmp(fn.exprs[fn.exprs[e].right].name, "value"));
        KirType other_phase = phase;
        KirModule other_provider = {0};
        other_provider.types = &other_phase;
        other_provider.type_count = 1;
        KirImport imports[2] = {import, import};
        imports[1].resolved_module = &other_provider;
        consumer.imports = imports;
        consumer.import_count = 2;
        e = KirParseExpr(&fn, &consumer, "(Phase)value", KirSpan("consumer.kry", 2, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_UNKNOWN);
        consumer.types = &phase;
        consumer.type_count = 1;
        e = KirParseExpr(&fn, &consumer, "(Phase)value", KirSpan("consumer.kry", 3, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_CAST);
    }
    {
        KirType record = {0};
        strcpy(record.name, "Props");
        KirModule module = {0};
        module.types = &record;
        module.type_count = 1;
        e = KirParseExpr(&fn, &module, "(Props){.count=next(), .active=true,}", KirSpan("props.kry", 1, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_COMPOUND && !strcmp(fn.exprs[e].name, "Props"));
        int field = fn.exprs[e].first_child;
        assert(fn.exprs[field].kind == KIR_EXPR_FIELD_INIT && !strcmp(fn.exprs[field].name, "count"));
        assert(fn.exprs[fn.exprs[field].right].kind == KIR_EXPR_CALL);
        field = fn.exprs[field].next_sibling;
        assert(!strcmp(fn.exprs[field].name, "active") && fn.exprs[field].next_sibling == -1);
        e = KirParseExpr(&fn, &module, "(Props){7, true}", KirSpan("props.kry", 2, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_COMPOUND);
        assert(fn.exprs[fn.exprs[e].first_child].name[0] == 0);
        e = KirParseExpr(&fn, &module, "(Props){.count=}", KirSpan("props.kry", 3, 1));
        assert(fn.exprs[e].kind == KIR_EXPR_UNKNOWN);
    }
    e = parse(&fn, "value++");
    assert(fn.exprs[e].kind == KIR_EXPR_POSTFIX);
    e = parse(&fn, ".5 + 0x1.fp-3");
    assert(fn.exprs[fn.exprs[e].left].kind == KIR_EXPR_FLOAT);
    assert(fn.exprs[fn.exprs[e].right].kind == KIR_EXPR_FLOAT);
    e = parse(&fn, "(Rectangle){0, 0, 1, 1}");
    assert(fn.exprs[e].kind == KIR_EXPR_COMPOUND);
    e = parse(&fn, "a +");
    assert(fn.exprs[e].kind == KIR_EXPR_UNKNOWN);
    e = parse(&fn, "x; injected()");
    assert(fn.exprs[e].kind == KIR_EXPR_UNKNOWN);
    {
        KirLexer lexer;
        KirLexerInit(&lexer, ":: := <<= >= ++ 1e-3 0xff", "tokens.kry");
        assert(!strcmp(KirLexerNext(&lexer).text, "::"));
        assert(!strcmp(KirLexerNext(&lexer).text, ":="));
        assert(!strcmp(KirLexerNext(&lexer).text, "<<="));
        assert(!strcmp(KirLexerNext(&lexer).text, ">="));
        assert(!strcmp(KirLexerNext(&lexer).text, "++"));
        assert(KirLexerNext(&lexer).kind == KIR_TOKEN_FLOAT);
        assert(KirLexerNext(&lexer).kind == KIR_TOKEN_INT);
    }
    free(fn.exprs);
    return 0;
}
