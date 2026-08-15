#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "kry_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int
clamp_size(int v, int lo, int hi)
{
    if(v < lo)
        return lo;
    if(v > hi)
        return hi;
    return v;
}

static void
clear_row(KryTerm *t, int y)
{
    int x;

    if(t->cells == NULL || y < 0 || y >= t->rows)
        return;
    for(x = 0; x < t->cols; x++)
        t->cells[y * t->cols + x] = ' ';
}

static void
clear_all(KryTerm *t)
{
    int y;

    for(y = 0; y < t->rows; y++)
        clear_row(t, y);
    t->cx = 0;
    t->cy = 0;
}

static int
alloc_cells(KryTerm *t, int cols, int rows)
{
    char *cells;
    int n;

    cols = clamp_size(cols, 8, KRY_TERM_MAX_COLS);
    rows = clamp_size(rows, 4, KRY_TERM_MAX_ROWS);
    n = cols * rows;
    cells = realloc(t->cells, (size_t)n);
    if(cells == NULL)
        return 0;
    t->cells = cells;
    t->cols = cols;
    t->rows = rows;
    clear_all(t);
    return 1;
}

static void
scroll_up(KryTerm *t)
{
    if(t->cells == NULL || t->rows < 2)
        return;
    memmove(t->cells, t->cells + t->cols, (size_t)((t->rows - 1) * t->cols));
    clear_row(t, t->rows - 1);
}

static void
put_char(KryTerm *t, unsigned char c)
{
    if(t->cells == NULL)
        return;
    if(t->cx >= t->cols) {
        t->cx = 0;
        t->cy++;
    }
    if(t->cy >= t->rows) {
        scroll_up(t);
        t->cy = t->rows - 1;
    }
    if(c < 32 || c == 127)
        c = '?';
    t->cells[t->cy * t->cols + t->cx] = (char)c;
    t->cx++;
}

static void
cursor_clamp(KryTerm *t)
{
    if(t->cx < 0)
        t->cx = 0;
    if(t->cy < 0)
        t->cy = 0;
    if(t->cx >= t->cols)
        t->cx = t->cols - 1;
    if(t->cy >= t->rows)
        t->cy = t->rows - 1;
}

static int
csi_arg(const KryTerm *t, int i, int fallback)
{
    if(i < 0 || i >= t->csi_n || t->csi[i] == 0)
        return fallback;
    return t->csi[i];
}

static void
apply_csi(KryTerm *t, int final)
{
    int n = csi_arg(t, 0, 1);
    int x;
    int y;

    if(final == 'A')
        t->cy -= n;
    else if(final == 'B')
        t->cy += n;
    else if(final == 'C')
        t->cx += n;
    else if(final == 'D')
        t->cx -= n;
    else if(final == 'G')
        t->cx = n - 1;
    else if(final == 'H' || final == 'f') {
        t->cy = csi_arg(t, 0, 1) - 1;
        t->cx = csi_arg(t, 1, 1) - 1;
    } else if(final == 'J') {
        int mode = csi_arg(t, 0, 0);

        if(mode == 2 || mode == 3)
            clear_all(t);
        else if(mode == 0) {
            for(x = t->cx; x < t->cols; x++)
                t->cells[t->cy * t->cols + x] = ' ';
            for(y = t->cy + 1; y < t->rows; y++)
                clear_row(t, y);
        }
    } else if(final == 'K') {
        int mode = csi_arg(t, 0, 0);
        int start = mode == 1 ? 0 : t->cx;
        int end = mode == 1 ? t->cx + 1 : t->cols;

        if(mode == 2) {
            start = 0;
            end = t->cols;
        }
        for(x = start; x < end; x++)
            t->cells[t->cy * t->cols + x] = ' ';
    }
    cursor_clamp(t);
}

static void
feed(KryTerm *t, unsigned char c)
{
    if(t->parse == 1) {
        if(c == '[') {
            t->parse = 2;
            t->csi_n = 0;
            t->csi[0] = 0;
            return;
        }
        t->parse = 0;
        return;
    }
    if(t->parse == 2) {
        if(c >= '0' && c <= '9') {
            if(t->csi_n == 0)
                t->csi_n = 1;
            t->csi[t->csi_n - 1] = t->csi[t->csi_n - 1] * 10 + (c - '0');
            return;
        }
        if(c == ';') {
            if(t->csi_n < 4) {
                if(t->csi_n == 0)
                    t->csi_n = 1;
                t->csi[t->csi_n] = 0;
                t->csi_n++;
            }
            return;
        }
        if(c == '?' || c == '>')
            return;
        t->parse = 0;
        if(c >= '@' && c <= '~')
            apply_csi(t, c);
        return;
    }
    if(c == 0x1b) {
        t->parse = 1;
        return;
    }
    if(c == '\r') {
        t->cx = 0;
        return;
    }
    if(c == '\n') {
        t->cy++;
        if(t->cy >= t->rows) {
            scroll_up(t);
            t->cy = t->rows - 1;
        }
        return;
    }
    if(c == '\b') {
        if(t->cx > 0)
            t->cx--;
        return;
    }
    if(c == '\t') {
        t->cx = (t->cx + 8) & ~7;
        if(t->cx >= t->cols)
            t->cx = t->cols - 1;
        return;
    }
    if(c == 7)
        return;
    put_char(t, c);
}

static void
set_winsize(int fd, int cols, int rows)
{
    struct winsize ws;

    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;
    ioctl(fd, TIOCSWINSZ, &ws);
}

int
KryTermSpawn(KryTerm *t, const char *cwd, int cols, int rows)
{
    int master;
    int pid;
    const char *shell;

    if(t == NULL)
        return 0;
    KryTermClose(t);
    if(!alloc_cells(t, cols, rows))
        return 0;
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if(master < 0)
        return 0;
    if(grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return 0;
    }
    set_winsize(master, t->cols, t->rows);
    pid = fork();
    if(pid < 0) {
        close(master);
        return 0;
    }
    if(pid == 0) {
        const char *slave_name = ptsname(master);
        char slave_path[256];
        int slave;

        if(slave_name == NULL)
            _exit(127);
        snprintf(slave_path, sizeof(slave_path), "%s", slave_name);
        setsid();
        close(master);
        slave = open(slave_path, O_RDWR);
        if(slave < 0)
            _exit(127);
#ifdef TIOCSCTTY
        ioctl(slave, TIOCSCTTY, 0);
#endif
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if(slave > 2)
            close(slave);
        if(cwd != NULL && cwd[0] != '\0') {
            if(chdir(cwd) != 0)
                _exit(127);
        }
        shell = getenv("SHELL");
        if(shell == NULL || shell[0] == '\0')
            shell = "/bin/sh";
        execl(shell, shell, "-i", (char *)NULL);
        _exit(127);
    }
    {
        int flags = fcntl(master, F_GETFL, 0);

        if(flags >= 0)
            fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    t->pid = pid;
    t->fd = master;
    t->running = 1;
    t->parse = 0;
    return 1;
}

int
KryTermWrite(KryTerm *t, const void *data, int n)
{
    int wrote;

    if(t == NULL || !t->running || t->fd <= 0 || data == NULL || n <= 0)
        return 0;
    wrote = (int)write(t->fd, data, (size_t)n);
    return wrote > 0 ? wrote : 0;
}

/* Print text into the terminal as if the child process emitted it:
 * same escape processing as polled output. Lets harnesses (the krait
 * agent) mirror their command output into the user's console pane. */
void
KryTermFeedOutput(KryTerm *t, const void *data, int n)
{
    const unsigned char *p = data;
    int i;

    if(t == NULL || p == NULL || n <= 0)
        return;
    for(i = 0; i < n; i++)
        feed(t, p[i]);
}

int
KryTermPoll(KryTerm *t)
{
    char buf[1024];
    int n;
    int i;
    int status;

    if(t == NULL || t->fd <= 0)
        return 0;
    for(;;) {
        n = (int)read(t->fd, buf, sizeof(buf));
        if(n < 0) {
            if(errno == EINTR)
                continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        if(n == 0)
            break;
        for(i = 0; i < n; i++)
            feed(t, (unsigned char)buf[i]);
    }
    if(t->running && t->pid > 0) {
        if(waitpid(t->pid, &status, WNOHANG) > 0)
            t->running = 0;
    }
    return t->running;
}

void
KryTermResize(KryTerm *t, int cols, int rows)
{
    if(t == NULL)
        return;
    cols = clamp_size(cols, 8, KRY_TERM_MAX_COLS);
    rows = clamp_size(rows, 4, KRY_TERM_MAX_ROWS);
    if(cols == t->cols && rows == t->rows)
        return;
    if(!alloc_cells(t, cols, rows))
        return;
    if(t->fd > 0)
        set_winsize(t->fd, t->cols, t->rows);
}

void
KryTermClose(KryTerm *t)
{
    int status;

    if(t == NULL)
        return;
    if(t->running && t->pid > 0) {
        kill(t->pid, SIGHUP);
        kill(t->pid, SIGTERM);
        while(waitpid(t->pid, &status, 0) < 0 && errno == EINTR)
            ;
    }
    if(t->fd > 0)
        close(t->fd);
    free(t->cells);
    memset(t, 0, sizeof(*t));
    t->fd = -1;
}

void
KryTermLine(const KryTerm *t, int row, char *dst, int dst_size)
{
    int x;
    int n;

    if(dst == NULL || dst_size <= 0)
        return;
    dst[0] = '\0';
    if(t == NULL || t->cells == NULL || row < 0 || row >= t->rows)
        return;
    n = t->cols;
    if(n > dst_size - 1)
        n = dst_size - 1;
    for(x = 0; x < n; x++)
        dst[x] = t->cells[row * t->cols + x];
    while(n > 0 && dst[n - 1] == ' ')
        n--;
    dst[n] = '\0';
}

#else

int
KryTermSpawn(KryTerm *t, const char *cwd, int cols, int rows)
{
    (void)cwd;
    (void)cols;
    (void)rows;
    if(t != NULL)
        memset(t, 0, sizeof(*t));
    return 0;
}

int
KryTermWrite(KryTerm *t, const void *data, int n)
{
    (void)t;
    (void)data;
    (void)n;
    return 0;
}

int
KryTermPoll(KryTerm *t)
{
    (void)t;
    return 0;
}

void
KryTermFeedOutput(KryTerm *t, const void *data, int n)
{
    (void)t;
    (void)data;
    (void)n;
}

void
KryTermResize(KryTerm *t, int cols, int rows)
{
    (void)t;
    (void)cols;
    (void)rows;
}

void
KryTermClose(KryTerm *t)
{
    if(t != NULL)
        memset(t, 0, sizeof(*t));
}

void
KryTermLine(const KryTerm *t, int row, char *dst, int dst_size)
{
    (void)t;
    (void)row;
    if(dst != NULL && dst_size > 0)
        dst[0] = '\0';
}

#endif
