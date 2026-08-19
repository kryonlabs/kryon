#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
    Terminal t;
    char line[128];
    const char *cmd = "printf 'TERMINAL_OK\\n'; exit\n";
    int i;
    int row;
    int saw = 0;

    memset(&t, 0, sizeof(t));
    if(!TerminalSpawn(&t, "/", 40, 8)) {
        fprintf(stderr, "spawn failed\n");
        return 1;
    }
    for(i = 0; i < 50; i++) {
        TerminalPoll(&t);
        for(row = 0; row < t.rows; row++) {
            TerminalLine(&t, row, line, sizeof(line));
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
    if(TerminalWrite(&t, cmd, (int)strlen(cmd)) <= 0) {
        TerminalClose(&t);
        fprintf(stderr, "write failed\n");
        return 1;
    }
    for(i = 0; i < 50; i++) {
        TerminalPoll(&t);
        for(row = 0; row < t.rows; row++) {
            TerminalLine(&t, row, line, sizeof(line));
            if(strstr(line, "TERMINAL_OK") != NULL) {
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
            TerminalLine(&t, row, line, sizeof(line));
            if(line[0] != '\0')
                fprintf(stderr, "row %d: %s\n", row, line);
        }
        TerminalClose(&t);
        return 1;
    }
    TerminalClose(&t);

    /* harness-printed text lands in the grid like process output */
    if(!TerminalSpawn(&t, "/", 80, 24)) {
        fprintf(stderr, "respawn failed\n");
        return 1;
    }
    for(i = 0; i < 20; i++)
        TerminalPoll(&t);
    TerminalFeedOutput(&t, "FEED_OK 42\n", 12);
    saw = 0;
    for(row = 0; row < t.rows; row++) {
        TerminalLine(&t, row, line, sizeof(line));
        if(strstr(line, "FEED_OK 42") != NULL)
            saw = 1;
    }
    if(!saw) {
        fprintf(stderr, "feed output not visible\n");
        TerminalClose(&t);
        return 1;
    }
    TerminalClose(&t);
    printf("ok term\n");
    return 0;
}
