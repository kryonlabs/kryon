#include "lawcheck.h"

#include <stdio.h>

void
lawcheck_begin(LawCheck *law, const char *name)
{
    law->current_law = name;
    printf("LAW %s\n", name);
}

void
lawcheck_require(LawCheck *law, int ok, const char *expr, const char *file,
                 int line)
{
    law->checks++;
    if(ok)
        return;

    law->failures++;
    if(law->failures <= 32) {
        fprintf(stderr, "%s:%d: law %s failed: %s\n",
                file, line, law->current_law, expr);
    } else if(law->failures == 33) {
        fprintf(stderr, "lawcheck: suppressing further failures\n");
    }
}

int
lawcheck_finish(LawCheck *law)
{
    if(law->failures != 0) {
        fprintf(stderr, "lawcheck: %d/%d checks failed\n",
                law->failures, law->checks);
        return 1;
    }

    printf("lawcheck: %d checks passed\n", law->checks);
    return 0;
}
