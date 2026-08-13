#ifndef KRY_TERM_H
#define KRY_TERM_H

/*
 * PTY-backed terminal for IDE hosts. Spawn a login shell, write keystrokes,
 * poll output, and keep a small screen grid. Windows is a no-op stub.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define KRY_TERM_MAX_COLS 240
#define KRY_TERM_MAX_ROWS 80

typedef struct KryTerm {
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
} KryTerm;

int KryTermSpawn(KryTerm *t, const char *cwd, int cols, int rows);
int KryTermWrite(KryTerm *t, const void *data, int n);
int KryTermPoll(KryTerm *t);
void KryTermResize(KryTerm *t, int cols, int rows);
void KryTermClose(KryTerm *t);
void KryTermLine(const KryTerm *t, int row, char *dst, int dst_size);

#ifdef __cplusplus
}
#endif

#endif
