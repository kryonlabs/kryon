#include "kry_term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
    KryTerm t;
    char line[128];
    int i;
    int saw = 0;

    memset(&t, 0, sizeof(t));
    if(!KryTermSpawn(&t, "/", 40, 8)) {
        fprintf(stderr, "spawn failed\n");
        return 1;
    }
    KryTermWrite(&t, "printf 'KRYTERM_OK\\n'; exit\n", 28);
    for(i = 0; i < 50; i++) {
        KryTermPoll(&t);
        KryTermLine(&t, 0, line, sizeof(line));
        if(strstr(line, "KRYTERM_OK") != NULL) {
            saw = 1;
            break;
        }
        usleep(20000);
    }
    KryTermClose(&t);
    if(!saw) {
        fprintf(stderr, "did not see marker\n");
        return 1;
    }
    printf("ok term\n");
    return 0;
}
