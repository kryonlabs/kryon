#include "kry_term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
    KryTerm t;
    char line[128];
    const char *cmd = "printf 'KRYTERM_OK\\n'; exit\n";
    int i;
    int row;
    int saw = 0;

    memset(&t, 0, sizeof(t));
    if(!KryTermSpawn(&t, "/", 40, 8)) {
        fprintf(stderr, "spawn failed\n");
        return 1;
    }
    for(i = 0; i < 50; i++) {
        KryTermPoll(&t);
        for(row = 0; row < t.rows; row++) {
            KryTermLine(&t, row, line, sizeof(line));
            if(line[0] != '\0') {
                saw = 1;
                break;
            }
        }
        if(saw)
            break;
        usleep(20000);
    }
    saw = 0;
    if(KryTermWrite(&t, cmd, (int)strlen(cmd)) <= 0) {
        KryTermClose(&t);
        fprintf(stderr, "write failed\n");
        return 1;
    }
    for(i = 0; i < 50; i++) {
        KryTermPoll(&t);
        for(row = 0; row < t.rows; row++) {
            KryTermLine(&t, row, line, sizeof(line));
            if(strstr(line, "KRYTERM_OK") != NULL) {
                saw = 1;
                break;
            }
        }
        if(saw)
            break;
        usleep(20000);
    }
    if(!saw) {
        fprintf(stderr, "did not see marker\n");
        for(row = 0; row < t.rows; row++) {
            KryTermLine(&t, row, line, sizeof(line));
            if(line[0] != '\0')
                fprintf(stderr, "row %d: %s\n", row, line);
        }
        KryTermClose(&t);
        return 1;
    }
    KryTermClose(&t);

    /* harness-printed text lands in the grid like process output */
    if(!KryTermSpawn(&t, "/", 80, 24)) {
        fprintf(stderr, "respawn failed\n");
        return 1;
    }
    for(i = 0; i < 20; i++)
        KryTermPoll(&t);
    KryTermFeedOutput(&t, "FEED_OK 42\n", 12);
    saw = 0;
    for(row = 0; row < t.rows; row++) {
        KryTermLine(&t, row, line, sizeof(line));
        if(strstr(line, "FEED_OK 42") != NULL)
            saw = 1;
    }
    if(!saw) {
        fprintf(stderr, "feed output not visible\n");
        KryTermClose(&t);
        return 1;
    }
    KryTermClose(&t);
    printf("ok term\n");
    return 0;
}
