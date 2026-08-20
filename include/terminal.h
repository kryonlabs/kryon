#ifndef TERMINAL_H
#define TERMINAL_H

/*
 * PTY-backed terminal for IDE hosts. Spawn a login shell, write keystrokes,
 * poll output, and keep a small screen grid. Windows is a no-op stub.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TERMINAL_MAX_COLS 240
#define TERMINAL_MAX_ROWS 80

typedef struct Terminal {
    int pid;
    int fd;
    int running;
    int cols;
    int rows;
    int cx;
    int cy;
    int parse;
    int csi[4];
    int csi_n;
    char *cells;
} Terminal;

int TerminalSpawn(Terminal *t, const char *cwd, int cols, int rows);
int TerminalWrite(Terminal *t, const void *data, int n);
int TerminalPoll(Terminal *t);
void TerminalFeedOutput(Terminal *t, const void *data, int n);
void TerminalResize(Terminal *t, int cols, int rows);
void TerminalClose(Terminal *t);
void TerminalLine(const Terminal *t, int row, char *dst, int dst_size);

#ifdef __cplusplus
}
#endif

#endif
