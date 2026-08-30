#include "app_storage.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void
check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %s: got %d expected %d\n", name, got, expected);
        failures++;
    }
}

static void
check_str(const char *name, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("FAIL %s: got '%s' expected '%s'\n", name, got, expected);
        failures++;
    }
}

int
main(void)
{
    char scope[64];
    char path[128];
    char out[32];
    int value;

    snprintf(scope, sizeof(scope), "app_storage_test_%ld", (long)getpid());
    snprintf(path, sizeof(path), ".kryon_%s_name.txt", scope);
    remove(path);
    snprintf(path, sizeof(path), ".kryon_%s_count.txt", scope);
    remove(path);

    check_int("missing string", KryAppStorageGetString(scope, "name", "fallback",
                                                       out, sizeof(out)), 0);
    check_str("missing fallback", out, "fallback");
    check_int("set string", KryAppStorageSetString(scope, "name", "stored"), 1);
    check_int("get string", KryAppStorageGetString(scope, "name", "fallback",
                                                   out, sizeof(out)), 1);
    check_str("stored string", out, "stored");

    check_int("set int", KryAppStorageSetInt(scope, "count", 42), 1);
    check_int("get int", KryAppStorageGetInt(scope, "count", 7, &value), 1);
    check_int("stored int", value, 42);
    check_int("missing int", KryAppStorageGetInt(scope, "missing", 7, &value), 0);
    check_int("missing int fallback", value, 7);

    snprintf(path, sizeof(path), ".kryon_%s_name.txt", scope);
    remove(path);
    snprintf(path, sizeof(path), ".kryon_%s_count.txt", scope);
    remove(path);
    return failures == 0 ? 0 : 1;
}
