#include "kir.h"

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

int
main(void)
{
    KirProgram *program;
    KirModule *module;
    KirFunction *fn;
    char buf[4096];
    size_t n;
    FILE *out;
    int ok = 1;

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
    ok &= check(KirFunctionAddWidget(fn, "Text", "\"Count\"",
                                     "Text(\"Count\")",
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
    ok &= check(strstr(buf, "stmt widget widget Text args \"Count\" text Text(\"Count\")") != NULL,
                "dump widget stmt");

    KirProgramFree(program);
    return ok ? 0 : 1;
}
