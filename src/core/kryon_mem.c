#include "kryon_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

static int
kryon_mem_debug_env(void)
{
    static int cached = -1;

    if(cached < 0) {
        const char *value = getenv("KRYON_MEM_DEBUG");

        cached = value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
    }
    return cached;
}

int
KryonMemDebugEnabled(void)
{
    return kryon_mem_debug_env();
}

void
KryonMemReport(const char *tag)
{
    if(!kryon_mem_debug_env())
        return;

    fprintf(stderr, "[kryon-mem] === %s ===\n", tag != NULL ? tag : "memory");
#if defined(__linux__)
    {
        FILE *status = fopen("/proc/self/status", "r");
        char line[256];

        if(status != NULL) {
            while(fgets(line, sizeof(line), status) != NULL) {
                if(strncmp(line, "VmRSS:", 6) == 0 ||
                   strncmp(line, "VmHWM:", 6) == 0 ||
                   strncmp(line, "VmData:", 7) == 0 ||
                   strncmp(line, "VmSwap:", 7) == 0)
                    fprintf(stderr, "[kryon-mem] %s", line);
            }
            fclose(status);
        }
    }
#endif
#if defined(__GLIBC__)
    /* Arena-by-arena breakdown; small constant output, stderr only. */
    malloc_stats();
#endif
    fflush(stderr);
}
