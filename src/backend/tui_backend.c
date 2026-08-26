#include "kry_input.h"
#include "kry_sw.h"
#include "kry_sw_png.h"
#include "kryon.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define KRY_TUI_MAX_TEXTURES 512
#define KRY_TUI_MAX_KEYS 512
#define KRY_TUI_KEY_QUEUE 64
#define KRY_TUI_CHAR_QUEUE 128
#define KRY_TUI_CELL_W 1
#define KRY_TUI_CELL_H 2

typedef struct KryTuiTexture {
    unsigned id;
    unsigned char *rgba;
    int width;
    int height;
    int owned_rgba;
    int render_target;
    KrySw sw;
    int sw_ready;
} KryTuiTexture;

typedef struct KryTuiCell {
    unsigned fg;
    unsigned bg;
} KryTuiCell;

static int g_width = 80;
static int g_height = 48;
static int g_term_cols = 80;
static int g_term_rows = 24;
static int g_ready;
static int g_should_close;
static int g_target_fps;
static int g_fps;
static int g_frame_counter;
static double g_fps_time;
static double g_last_time;
static float g_frame_time = 1.0f / 60.0f;
static unsigned g_window_state;
static int g_exit_key = KEY_ESCAPE;
static int g_raw_fd = -1;
static int g_out_fd = -1;
static int g_termios_saved;
static struct termios g_saved_termios;
static int g_saved_flags = -1;
static int g_alt_screen;
static volatile sig_atomic_t g_resize_pending;
static KrySw g_sw;
static KrySw *g_active_sw;
static const KryBackend *g_sw_backend;
static int g_sw_ready;
static unsigned g_active_texture_id;
static unsigned g_target_stack[16];
static int g_target_depth;
static KryTuiTexture g_textures[KRY_TUI_MAX_TEXTURES];
static unsigned g_next_texture_id = 1;
static KryTuiCell *g_cells;
static int g_cells_cols;
static int g_cells_rows;
static char *g_clipboard;
static Font g_default_font;
static int g_default_ready;

static int g_key_down[KRY_TUI_MAX_KEYS];
static int g_key_pressed[KRY_TUI_MAX_KEYS];
static int g_key_released[KRY_TUI_MAX_KEYS];
static int g_key_queue[KRY_TUI_KEY_QUEUE];
static int g_key_qr;
static int g_key_qw;
static int g_char_queue[KRY_TUI_CHAR_QUEUE];
static int g_char_qr;
static int g_char_qw;
static int g_mouse_x;
static int g_mouse_y;
static int g_mouse_dx;
static int g_mouse_dy;
static int g_mouse_down[3];
static int g_mouse_pressed[3];
static int g_mouse_released[3];
static int g_wheel;

extern unsigned char *kry_decode_image_rgba(const unsigned char *data, int len,
                                            int *width, int *height);
extern int kry_write_png_file(const char *path, const unsigned char *rgba,
                              int w, int h);

static double
now_seconds(void)
{
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
    return (double)time(NULL);
}

static unsigned
pack(Color c)
{
    return ((unsigned)c.r << 24) | ((unsigned)c.g << 16) |
           ((unsigned)c.b << 8) | (unsigned)c.a;
}

static int
abs_int(int v)
{
    return v < 0 ? -v : v;
}

static int
clampi(int v, int lo, int hi)
{
    if(v < lo)
        return lo;
    if(v > hi)
        return hi;
    return v;
}

static void
write_all(const char *s, size_t n)
{
    while(n > 0 && g_out_fd >= 0) {
        ssize_t w = write(g_out_fd, s, n);

        if(w <= 0) {
            if(errno == EINTR)
                continue;
            break;
        }
        s += w;
        n -= (size_t)w;
    }
}

static void
write_cstr(const char *s)
{
    if(s != NULL)
        write_all(s, strlen(s));
}

static void
query_terminal_size(void)
{
    struct winsize ws;

    if(g_raw_fd >= 0 && ioctl(g_raw_fd, TIOCGWINSZ, &ws) == 0 &&
       ws.ws_col > 0 && ws.ws_row > 0) {
        g_term_cols = ws.ws_col;
        g_term_rows = ws.ws_row;
    }
    if(g_term_cols <= 0)
        g_term_cols = 80;
    if(g_term_rows <= 0)
        g_term_rows = 24;
    g_width = g_term_cols * KRY_TUI_CELL_W;
    g_height = g_term_rows * KRY_TUI_CELL_H;
}

static void
sigwinch_handler(int sig)
{
    (void)sig;
    g_resize_pending = 1;
}

static void
restore_terminal(void)
{
    if(g_alt_screen && g_out_fd >= 0) {
        write_cstr("\033[?1006l\033[?1003l\033[?1002l\033[?1000l");
        write_cstr("\033[?25h\033[0m\033[?1049l");
        g_alt_screen = 0;
    }
    if(g_raw_fd >= 0 && g_termios_saved) {
        tcsetattr(g_raw_fd, TCSANOW, &g_saved_termios);
        g_termios_saved = 0;
    }
    if(g_raw_fd >= 0 && g_saved_flags >= 0) {
        fcntl(g_raw_fd, F_SETFL, g_saved_flags);
        g_saved_flags = -1;
    }
}

static int
setup_terminal(const char *title)
{
    struct termios raw;

    g_raw_fd = STDIN_FILENO;
    g_out_fd = STDOUT_FILENO;
    if(!isatty(g_raw_fd) || !isatty(g_out_fd)) {
        const char *force = getenv("KRYON_TUI_FORCE");

        if(force == NULL || force[0] == '\0')
            return -1;
    }
    query_terminal_size();
    if(tcgetattr(g_raw_fd, &g_saved_termios) == 0) {
        raw = g_saved_termios;
        raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= (tcflag_t)~(OPOST);
        raw.c_cflag |= CS8;
        raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if(tcsetattr(g_raw_fd, TCSANOW, &raw) == 0)
            g_termios_saved = 1;
    }
    g_saved_flags = fcntl(g_raw_fd, F_GETFL, 0);
    if(g_saved_flags >= 0)
        fcntl(g_raw_fd, F_SETFL, g_saved_flags | O_NONBLOCK);
    signal(SIGWINCH, sigwinch_handler);
    atexit(restore_terminal);
    write_cstr("\033[?1049h\033[?25l\033[?1000h\033[?1002h\033[?1006h\033[2J");
    if(title != NULL && title[0] != '\0') {
        write_cstr("\033]0;");
        write_cstr(title);
        write_cstr("\007");
    }
    g_alt_screen = 1;
    return 0;
}

static int
ensure_sw(int width, int height)
{
    if(width <= 0)
        width = 80;
    if(height <= 0)
        height = 48;
    if(g_sw_ready && g_sw.w == width && g_sw.h == height)
        return 0;
    if(g_sw_ready)
        KrySwFree(&g_sw);
    if(KrySwInit(&g_sw, NULL, width, height) != 0) {
        g_sw_ready = 0;
        return -1;
    }
    g_sw_backend = KrySwBackend(&g_sw);
    g_active_sw = &g_sw;
    g_active_texture_id = 0;
    g_sw_ready = 1;
    return 0;
}

static void
set_active_sw(KrySw *sw, unsigned texture_id)
{
    if(sw == NULL)
        return;
    g_sw_backend = KrySwBackend(sw);
    g_active_sw = sw;
    g_active_texture_id = texture_id;
}

static void
free_cells(void)
{
    free(g_cells);
    g_cells = NULL;
    g_cells_cols = 0;
    g_cells_rows = 0;
}

static int
ensure_cells(int cols, int rows)
{
    size_t n;

    if(cols <= 0 || rows <= 0)
        return -1;
    if(g_cells != NULL && g_cells_cols == cols && g_cells_rows == rows)
        return 0;
    n = (size_t)cols * (size_t)rows;
    free_cells();
    g_cells = malloc(n * sizeof(*g_cells));
    if(g_cells == NULL)
        return -1;
    memset(g_cells, 0xff, n * sizeof(*g_cells));
    g_cells_cols = cols;
    g_cells_rows = rows;
    write_cstr("\033[2J");
    return 0;
}

static unsigned
pixel_at(const KrySw *sw, int x, int y)
{
    const unsigned char *p;

    if(sw == NULL || sw->pixels == NULL || x < 0 || y < 0 ||
       x >= sw->w || y >= sw->h)
        return 0x000000ffu;
    p = sw->pixels + (size_t)y * sw->stride + (size_t)x * 4;
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | (unsigned)p[3];
}

static void
emit_cell(int col, int row, unsigned fg, unsigned bg)
{
    char buf[128];
    int n;

    n = snprintf(buf, sizeof(buf),
                 "\033[%d;%dH\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um\342\226\200",
                 row + 1, col + 1, (fg >> 24) & 255, (fg >> 16) & 255,
                 (fg >> 8) & 255, (bg >> 24) & 255, (bg >> 16) & 255,
                 (bg >> 8) & 255);
    if(n > 0)
        write_all(buf, (size_t)n);
}

static void
present_sw(void)
{
    int cols;
    int rows;
    int x;
    int y;

    if(!g_sw_ready || g_active_sw != &g_sw || g_out_fd < 0)
        return;
    cols = (g_sw.w + KRY_TUI_CELL_W - 1) / KRY_TUI_CELL_W;
    rows = (g_sw.h + KRY_TUI_CELL_H - 1) / KRY_TUI_CELL_H;
    if(ensure_cells(cols, rows) != 0)
        return;
    for(y = 0; y < rows; y++) {
        for(x = 0; x < cols; x++) {
            unsigned fg = pixel_at(&g_sw, x, y * 2);
            unsigned bg = pixel_at(&g_sw, x, y * 2 + 1);
            KryTuiCell *cell = &g_cells[(size_t)y * cols + x];

            if(cell->fg == fg && cell->bg == bg)
                continue;
            emit_cell(x, y, fg, bg);
            cell->fg = fg;
            cell->bg = bg;
        }
    }
    write_cstr("\033[0m");
}

static void
reset_edges(void)
{
    memset(g_key_pressed, 0, sizeof(g_key_pressed));
    memset(g_key_released, 0, sizeof(g_key_released));
    memset(g_mouse_pressed, 0, sizeof(g_mouse_pressed));
    memset(g_mouse_released, 0, sizeof(g_mouse_released));
    g_mouse_dx = 0;
    g_mouse_dy = 0;
    g_wheel = 0;
}

static void
push_key(int key)
{
    if(key <= 0)
        return;
    if(key < KRY_TUI_MAX_KEYS) {
        if(!g_key_down[key])
            g_key_pressed[key] = 1;
        g_key_down[key] = 1;
    }
    g_key_queue[g_key_qw++ % KRY_TUI_KEY_QUEUE] = key;
}

static void
release_key(int key)
{
    if(key > 0 && key < KRY_TUI_MAX_KEYS) {
        g_key_down[key] = 0;
        g_key_released[key] = 1;
    }
}

static void
tap_key(int key)
{
    push_key(key);
    release_key(key);
}

static void
push_char(int ch)
{
    if(ch > 0)
        g_char_queue[g_char_qw++ % KRY_TUI_CHAR_QUEUE] = ch;
}

static int
utf8_decode_char(const unsigned char *buf, int len, int *used)
{
    unsigned c;

    if(len <= 0)
        return 0;
    if(buf[0] < 0x80) {
        *used = 1;
        return buf[0];
    }
    if((buf[0] & 0xe0) == 0xc0 && len >= 2) {
        c = ((unsigned)(buf[0] & 0x1f) << 6) | (unsigned)(buf[1] & 0x3f);
        *used = 2;
        return (int)c;
    }
    if((buf[0] & 0xf0) == 0xe0 && len >= 3) {
        c = ((unsigned)(buf[0] & 0x0f) << 12) |
            ((unsigned)(buf[1] & 0x3f) << 6) | (unsigned)(buf[2] & 0x3f);
        *used = 3;
        return (int)c;
    }
    if((buf[0] & 0xf8) == 0xf0 && len >= 4) {
        c = ((unsigned)(buf[0] & 0x07) << 18) |
            ((unsigned)(buf[1] & 0x3f) << 12) |
            ((unsigned)(buf[2] & 0x3f) << 6) | (unsigned)(buf[3] & 0x3f);
        *used = 4;
        return (int)c;
    }
    *used = 1;
    return 0;
}

static int
csi_key(int final, const int *params, int nparams)
{
    int code = nparams > 0 ? params[0] : 0;

    if(final == 'A')
        return KEY_UP;
    if(final == 'B')
        return KEY_DOWN;
    if(final == 'C')
        return KEY_RIGHT;
    if(final == 'D')
        return KEY_LEFT;
    if(final == 'H')
        return KEY_HOME;
    if(final == 'F')
        return KEY_END;
    if(final == '~') {
        switch(code) {
        case 1:
        case 7:
            return KEY_HOME;
        case 2:
            return KEY_INSERT;
        case 3:
            return KEY_DELETE;
        case 4:
        case 8:
            return KEY_END;
        case 5:
            return KEY_PAGE_UP;
        case 6:
            return KEY_PAGE_DOWN;
        case 11:
            return KEY_F1;
        case 12:
            return KEY_F2;
        case 13:
            return KEY_F3;
        case 14:
            return KEY_F4;
        case 15:
            return KEY_F5;
        case 17:
            return KEY_F6;
        case 18:
            return KEY_F7;
        case 19:
            return KEY_F8;
        case 20:
            return KEY_F9;
        case 21:
            return KEY_F10;
        case 23:
            return KEY_F11;
        case 24:
            return KEY_F12;
        default:
            break;
        }
    }
    return 0;
}

static void
handle_sgr_mouse(const int *params, int nparams, int release)
{
    int b;
    int x;
    int y;
    int button;

    if(nparams < 3)
        return;
    b = params[0];
    x = params[1] - 1;
    y = params[2] - 1;
    x = clampi(x, 0, g_term_cols - 1);
    y = clampi(y, 0, g_term_rows - 1);
    g_mouse_dx += x - g_mouse_x;
    g_mouse_dy += y * 2 - g_mouse_y;
    g_mouse_x = x;
    g_mouse_y = y * 2;
    if((b & 64) != 0) {
        g_wheel += (b & 1) ? -1 : 1;
        return;
    }
    button = b & 3;
    if(button >= 0 && button < 3) {
        if(release) {
            g_mouse_down[button] = 0;
            g_mouse_released[button] = 1;
        } else if(!g_mouse_down[button]) {
            g_mouse_down[button] = 1;
            g_mouse_pressed[button] = 1;
        }
    }
}

static int
parse_csi(const unsigned char *buf, int len)
{
    int params[8] = {0};
    int nparams = 0;
    int value = 0;
    int have = 0;
    int sgr = 0;
    int i;

    if(len < 3 || buf[0] != 0x1b || buf[1] != '[')
        return 0;
    for(i = 2; i < len; i++) {
        int ch = buf[i];

        if(ch == '<' && i == 2) {
            sgr = 1;
            continue;
        }
        if(ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            have = 1;
            continue;
        }
        if(ch == ';') {
            if(nparams < 8)
                params[nparams++] = have ? value : 0;
            value = 0;
            have = 0;
            continue;
        }
        if(nparams < 8)
            params[nparams++] = have ? value : 0;
        if(sgr && (ch == 'M' || ch == 'm')) {
            handle_sgr_mouse(params, nparams, ch == 'm');
            return i + 1;
        } else {
            int key = csi_key(ch, params, nparams);

            if(key != 0)
                tap_key(key);
            return i + 1;
        }
    }
    return 0;
}

static void
process_input_bytes(const unsigned char *buf, int len)
{
    int i = 0;

    while(i < len) {
        int used = 1;
        int cp;

        if(buf[i] == 0x1b) {
            int n = parse_csi(buf + i, len - i);

            if(n > 0) {
                i += n;
                continue;
            }
            tap_key(KEY_ESCAPE);
            i++;
            continue;
        }
        if(buf[i] == '\r' || buf[i] == '\n') {
            tap_key(KEY_ENTER);
            push_char('\n');
            i++;
            continue;
        }
        if(buf[i] == '\t') {
            tap_key(KEY_TAB);
            push_char('\t');
            i++;
            continue;
        }
        if(buf[i] == 0x7f || buf[i] == 0x08) {
            tap_key(KEY_BACKSPACE);
            push_char(0x08);
            i++;
            continue;
        }
        cp = utf8_decode_char(buf + i, len - i, &used);
        if(cp >= 32) {
            if(cp < 128)
                tap_key(cp >= 'a' && cp <= 'z' ? cp - 32 : cp);
            push_char(cp);
        }
        i += used > 0 ? used : 1;
    }
}

static void
poll_terminal(void)
{
    unsigned char buf[512];

    if(g_resize_pending) {
        g_resize_pending = 0;
        query_terminal_size();
        ensure_sw(g_width, g_height);
        free_cells();
    }
    for(;;) {
        ssize_t n;

        if(g_raw_fd < 0)
            return;
        n = read(g_raw_fd, buf, sizeof(buf));
        if(n > 0) {
            process_input_bytes(buf, (int)n);
            continue;
        }
        if(n < 0 && errno == EINTR)
            continue;
        break;
    }
}

void
KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    g_should_close = 0;
    if(setup_terminal(title) != 0) {
        g_ready = 0;
        return;
    }
    if(width > 0 && height > 0) {
        g_width = width;
        g_height = height;
        g_term_cols = (width + 1) / 1;
        g_term_rows = (height + 1) / 2;
    }
    if(ensure_sw(g_width, g_height) != 0) {
        g_ready = 0;
        return;
    }
    g_ready = 1;
    g_last_time = now_seconds();
}

void
KryonRaylibBackend_CloseWindow(void)
{
    int i;

    restore_terminal();
    free_cells();
    free(g_clipboard);
    g_clipboard = NULL;
    for(i = 1; i < KRY_TUI_MAX_TEXTURES; i++) {
        if(g_textures[i].sw_ready)
            KrySwFree(&g_textures[i].sw);
        if(g_textures[i].owned_rgba)
            free(g_textures[i].rgba);
        memset(&g_textures[i], 0, sizeof(g_textures[i]));
    }
    if(g_sw_ready) {
        KrySwFree(&g_sw);
        g_sw_ready = 0;
    }
    g_ready = 0;
}

bool
KryonRaylibBackend_WindowShouldClose(void)
{
    poll_terminal();
    if(g_exit_key > 0 && g_exit_key < KRY_TUI_MAX_KEYS &&
       g_key_pressed[g_exit_key])
        g_should_close = 1;
    return g_should_close != 0;
}

void
KryonRaylibBackend_EndDrawing(void)
{
    double now;

    present_sw();
    now = now_seconds();
    if(g_target_fps > 0 && g_last_time > 0.0) {
        double target = 1.0 / (double)g_target_fps;
        double elapsed = now - g_last_time;

        if(elapsed < target) {
            usleep((useconds_t)((target - elapsed) * 1000000.0));
            now = now_seconds();
        }
    }
    g_frame_time = (float)(now - g_last_time);
    if(g_frame_time <= 0.0f)
        g_frame_time = 1.0f / 60.0f;
    g_last_time = now;
    g_frame_counter++;
    if(g_fps_time <= 0.0)
        g_fps_time = now;
    if(now - g_fps_time >= 1.0) {
        g_fps = g_frame_counter;
        g_frame_counter = 0;
        g_fps_time = now;
    }
    reset_edges();
}

bool IsWindowReady(void) { return g_ready != 0; }
bool IsWindowFocused(void) { return g_ready != 0; }
bool IsWindowFullscreen(void) { return (g_window_state & FLAG_FULLSCREEN_MODE) != 0; }
bool IsWindowHidden(void) { return (g_window_state & FLAG_WINDOW_HIDDEN) != 0; }
bool IsWindowMinimized(void) { return (g_window_state & FLAG_WINDOW_MINIMIZED) != 0; }
bool IsWindowMaximized(void) { return (g_window_state & FLAG_WINDOW_MAXIMIZED) != 0; }
bool IsWindowResized(void) { return g_resize_pending != 0; }
bool IsWindowState(unsigned int flag) { return (g_window_state & flag) != 0; }
void SetWindowState(unsigned int flags) { g_window_state |= flags; }
void ClearWindowState(unsigned int flags) { g_window_state &= ~flags; }
void ToggleFullscreen(void) { g_window_state ^= FLAG_FULLSCREEN_MODE; }
void MaximizeWindow(void) { g_window_state |= FLAG_WINDOW_MAXIMIZED; }
void MinimizeWindow(void) { g_window_state |= FLAG_WINDOW_MINIMIZED; }
void RestoreWindow(void) { g_window_state &= ~(FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED); }
void SetWindowSize(int width, int height) { g_width = width; g_height = height; ensure_sw(width, height); free_cells(); }
void *GetWindowHandle(void) { return NULL; }
int GetScreenWidth(void) { return g_width; }
int GetScreenHeight(void) { return g_height; }
int GetRenderWidth(void) { return g_width; }
int GetRenderHeight(void) { return g_height; }
int GetCurrentMonitor(void) { return 0; }
int GetMonitorCount(void) { return 1; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){0, 0}; }
int GetMonitorWidth(int monitor) { (void)monitor; return g_width; }
int GetMonitorHeight(int monitor) { (void)monitor; return g_height; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){1.0f, 1.0f}; }
float GetFrameTime(void) { return g_frame_time; }
double GetTime(void) { return now_seconds(); }
int GetFPS(void) { return g_fps > 0 ? g_fps : (g_frame_time > 0 ? (int)(1.0f / g_frame_time + 0.5f) : 0); }
void SetTargetFPS(int fps) { g_target_fps = fps > 0 ? fps : 0; }
void SetConfigFlags(unsigned int flags) { g_window_state |= flags; }
void SetTraceLogLevel(int logLevel) { (void)logLevel; }
void SetMouseCursor(int cursor) { (void)cursor; }
void SetExitKey(int key) { g_exit_key = key; }
void WaitTime(double seconds) { if(seconds > 0.0) usleep((useconds_t)(seconds * 1000000.0)); }
void BeginDrawing(void) { poll_terminal(); }
void SetWindowTitle(const char *title) { (void)title; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height) { (void)width; (void)height; }
void SetWindowMaxSize(int width, int height) { (void)width; (void)height; }
void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) {}
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }

static void sw_rect(int x, int y, int w, int h, Color color)
{
    if(g_sw_backend != NULL)
        g_sw_backend->rect(x, y, w, h, pack(color));
}

void ClearBackground(Color color) { if(g_sw_backend != NULL) g_sw_backend->clear(pack(color)); }
void DrawRectangle(int posX, int posY, int width, int height, Color color) { sw_rect(posX, posY, width, height, color); }
void DrawRectangleRec(Rectangle rec, Color color) { sw_rect((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, color); }
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
    sw_rect(posX, posY, width, 1, color);
    sw_rect(posX, posY + height - 1, width, 1, color);
    sw_rect(posX, posY, 1, height, color);
    sw_rect(posX + width - 1, posY, 1, height, color);
}
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    int t = lineThick > 1.0f ? (int)(lineThick + 0.5f) : 1;

    sw_rect((int)rec.x, (int)rec.y, (int)rec.width, t, color);
    sw_rect((int)rec.x, (int)(rec.y + rec.height) - t, (int)rec.width, t, color);
    sw_rect((int)rec.x, (int)rec.y, t, (int)rec.height, color);
    sw_rect((int)(rec.x + rec.width) - t, (int)rec.y, t, (int)rec.height, color);
}
void DrawRectangleGradientV(int x, int y, int w, int h, Color top, Color bottom)
{
    int i;

    for(i = 0; i < h; i++) {
        float a = h > 1 ? (float)i / (float)(h - 1) : 0.0f;
        Color c = {(unsigned char)(top.r + (bottom.r - top.r) * a),
                   (unsigned char)(top.g + (bottom.g - top.g) * a),
                   (unsigned char)(top.b + (bottom.b - top.b) * a),
                   (unsigned char)(top.a + (bottom.a - top.a) * a)};
        sw_rect(x, y + i, w, 1, c);
    }
}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
    (void)roundness;
    (void)segments;
    DrawRectangleRec(rec, color);
}
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, Color color)
{
    (void)roundness;
    (void)segments;
    DrawRectangleLines((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, color);
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color)
{
    (void)roundness;
    (void)segments;
    DrawRectangleLinesEx(rec, lineThick, color);
}
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    if(g_sw_backend != NULL && g_sw_backend->circle != NULL)
        g_sw_backend->circle(centerX, centerY, (int)radius, pack(color));
}
void DrawCircleV(Vector2 center, float radius, Color color) { DrawCircle((int)center.x, (int)center.y, radius, color); }
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    DrawRing((Vector2){(float)centerX, (float)centerY}, radius - 1.0f, radius, 0.0f, 360.0f, 32, color);
}
void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
    (void)startAngle;
    (void)endAngle;
    (void)segments;
    if(g_sw_backend != NULL && g_sw_backend->ring != NULL)
        g_sw_backend->ring((int)center.x, (int)center.y, (int)innerRadius, (int)outerRadius, pack(color));
}
void DrawLine(int x1, int y1, int x2, int y2, Color color)
{
    int dx = abs_int(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs_int(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    for(;;) {
        sw_rect(x1, y1, 1, 1, color);
        if(x1 == x2 && y1 == y2)
            break;
        if(2 * err >= dy) {
            err += dy;
            x1 += sx;
        }
        if(2 * err <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}
void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color)
{
    int t = thick > 1.0f ? (int)(thick + 0.5f) : 1;

    if(abs_int((int)(end.x - start.x)) >= abs_int((int)(end.y - start.y)))
        DrawLine((int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
    else
        DrawLine((int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
    if(t > 1)
        sw_rect((int)start.x, (int)start.y, t, t, color);
}
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    DrawLine((int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, color);
    DrawLine((int)v2.x, (int)v2.y, (int)v3.x, (int)v3.y, color);
    DrawLine((int)v3.x, (int)v3.y, (int)v1.x, (int)v1.y, color);
}
void BeginScissorMode(int x, int y, int width, int height)
{
    if(g_sw_backend != NULL)
        g_sw_backend->clip_push(x, y, width, height);
}
void EndScissorMode(void) { if(g_sw_backend != NULL) g_sw_backend->clip_pop(); }
void BeginMode2D(Camera2D camera) { (void)camera; }
void EndMode2D(void) {}

static unsigned
register_texture(unsigned char *rgba, int width, int height, int owned_rgba)
{
    int i;
    unsigned id = g_next_texture_id++;

    if(id == 0)
        id = g_next_texture_id++;
    for(i = 1; i < KRY_TUI_MAX_TEXTURES; i++) {
        if(g_textures[i].id == 0) {
            g_textures[i].id = id;
            g_textures[i].rgba = rgba;
            g_textures[i].width = width;
            g_textures[i].height = height;
            g_textures[i].owned_rgba = owned_rgba;
            return id;
        }
    }
    return 0;
}

static KryTuiTexture *
texture_for(unsigned id)
{
    int i;

    for(i = 1; i < KRY_TUI_MAX_TEXTURES; i++)
        if(g_textures[i].id == id)
            return &g_textures[i];
    return NULL;
}

static void
unregister_texture(unsigned id)
{
    KryTuiTexture *t = texture_for(id);

    if(t == NULL)
        return;
    if(t->sw_ready)
        KrySwFree(&t->sw);
    if(t->owned_rgba)
        free(t->rgba);
    memset(t, 0, sizeof(*t));
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize)
{
    Image img = {0};

    (void)fileType;
    img.data = kry_sw_png_rgba(fileData, (size_t)dataSize, &img.width, &img.height);
    if(img.data == NULL)
        img.data = kry_decode_image_rgba(fileData, dataSize, &img.width, &img.height);
    if(img.data != NULL) {
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    }
    return img;
}

unsigned char *LoadFileData(const char *fileName, int *dataSize)
{
    FILE *f;
    long n;
    unsigned char *data;

    if(dataSize != NULL)
        *dataSize = 0;
    if(fileName == NULL)
        return NULL;
    f = fopen(fileName, "rb");
    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)n + 1);
    if(data == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    data[n] = 0;
    if(dataSize != NULL)
        *dataSize = (int)n;
    return data;
}
void UnloadFileData(unsigned char *data) { free(data); }
char *LoadFileText(const char *fileName) { int n = 0; return (char *)LoadFileData(fileName, &n); }
void UnloadFileText(char *text) { free(text); }
bool SaveFileData(const char *fileName, const void *data, int dataSize)
{
    FILE *f;

    if(fileName == NULL || data == NULL || dataSize < 0)
        return false;
    f = fopen(fileName, "wb");
    if(f == NULL)
        return false;
    if(fwrite(data, 1, (size_t)dataSize, f) != (size_t)dataSize) {
        fclose(f);
        return false;
    }
    return fclose(f) == 0;
}
bool SaveFileText(const char *fileName, const char *text)
{
    return SaveFileData(fileName, text != NULL ? text : "", text != NULL ? (int)strlen(text) : 0);
}

Image LoadImage(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    const char *ext = fileName != NULL ? strrchr(fileName, '.') : NULL;
    Image img = LoadImageFromMemory(ext != NULL ? ext : "", data, len);

    free(data);
    return img;
}
Image GenImageColor(int width, int height, Color color)
{
    Image img = {0};
    unsigned char *p;
    int i;

    if(width <= 0 || height <= 0)
        return img;
    p = malloc((size_t)width * height * 4);
    if(p == NULL)
        return img;
    for(i = 0; i < width * height; i++) {
        p[i * 4 + 0] = color.r;
        p[i * 4 + 1] = color.g;
        p[i * 4 + 2] = color.b;
        p[i * 4 + 3] = color.a;
    }
    img.data = p;
    img.width = width;
    img.height = height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}
void UnloadImage(Image image) { free(image.data); }
void ImageFormat(Image *image, int newFormat) { if(image != NULL) image->format = newFormat; }
void ImageFlipVertical(Image *image)
{
    int y;
    unsigned char *tmp;

    if(image == NULL || image->data == NULL || image->height <= 1)
        return;
    tmp = malloc((size_t)image->width * 4);
    if(tmp == NULL)
        return;
    for(y = 0; y < image->height / 2; y++) {
        unsigned char *a = (unsigned char *)image->data + (size_t)y * image->width * 4;
        unsigned char *b = (unsigned char *)image->data + (size_t)(image->height - 1 - y) * image->width * 4;

        memcpy(tmp, a, (size_t)image->width * 4);
        memcpy(a, b, (size_t)image->width * 4);
        memcpy(b, tmp, (size_t)image->width * 4);
    }
    free(tmp);
}
Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex = {0};
    unsigned char *copy;

    if(image.data == NULL || image.width <= 0 || image.height <= 0)
        return tex;
    copy = malloc((size_t)image.width * image.height * 4);
    if(copy == NULL)
        return tex;
    memcpy(copy, image.data, (size_t)image.width * image.height * 4);
    tex.id = register_texture(copy, image.width, image.height, 1);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = image.format;
    if(tex.id == 0) {
        free(copy);
        memset(&tex, 0, sizeof(tex));
    }
    return tex;
}
Texture2D LoadTexture(const char *fileName)
{
    Image img = LoadImage(fileName);
    Texture2D tex = LoadTextureFromImage(img);

    UnloadImage(img);
    return tex;
}
void UnloadTexture(Texture2D texture) { unregister_texture(texture.id); }
RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D rt = {0};
    Image img = GenImageColor(width, height, BLANK);
    KryTuiTexture *t;

    rt.texture = LoadTextureFromImage(img);
    rt.id = rt.texture.id;
    t = texture_for(rt.texture.id);
    if(t != NULL) {
        t->render_target = 1;
        if(KrySwInit(&t->sw, t->rgba, t->width, t->height) == 0)
            t->sw_ready = 1;
    }
    UnloadImage(img);
    return rt;
}
void UnloadRenderTexture(RenderTexture2D target) { UnloadTexture(target.texture); }
void BeginTextureMode(RenderTexture2D target)
{
    KryTuiTexture *t = texture_for(target.texture.id);

    if(t == NULL || t->rgba == NULL)
        return;
    if(!t->sw_ready && KrySwInit(&t->sw, t->rgba, t->width, t->height) != 0)
        return;
    t->sw_ready = 1;
    if(g_target_depth < 16)
        g_target_stack[g_target_depth++] = g_active_texture_id;
    set_active_sw(&t->sw, t->id);
}
void EndTextureMode(void)
{
    unsigned restore_id;
    KryTuiTexture *t;

    if(g_target_depth <= 0)
        return;
    restore_id = g_target_stack[--g_target_depth];
    if(restore_id == 0) {
        set_active_sw(&g_sw, 0);
        return;
    }
    t = texture_for(restore_id);
    if(t != NULL && t->sw_ready)
        set_active_sw(&t->sw, t->id);
}
Image LoadImageFromTexture(Texture2D texture)
{
    KryTuiTexture *t = texture_for(texture.id);
    Image img = {0};

    if(t == NULL || t->rgba == NULL)
        return img;
    img.data = malloc((size_t)t->width * t->height * 4);
    if(img.data == NULL)
        return img;
    memcpy(img.data, t->rgba, (size_t)t->width * t->height * 4);
    img.width = t->width;
    img.height = t->height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}
bool ExportImage(Image image, const char *fileName)
{
    return image.data != NULL && fileName != NULL &&
           kry_write_png_file(fileName, image.data, image.width, image.height) == 0;
}
int kry_backend_capture_screen(Image *img)
{
    if(img == NULL || !g_sw_ready)
        return -1;
    memset(img, 0, sizeof(*img));
    img->data = malloc((size_t)g_sw.w * g_sw.h * 4);
    if(img->data == NULL)
        return -1;
    memcpy(img->data, g_sw.pixels, (size_t)g_sw.w * g_sw.h * 4);
    img->width = g_sw.w;
    img->height = g_sw.h;
    img->mipmaps = 1;
    img->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return 0;
}

void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width, (float)texture.height},
                   (Rectangle){(float)posX, (float)posY, (float)texture.width, (float)texture.height},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    KryTuiTexture *t = texture_for(texture.id);
    unsigned char *region;
    int sw;
    int sh;
    int x;
    int y;

    (void)origin;
    (void)rotation;
    if(t == NULL || t->rgba == NULL || g_sw_backend == NULL || g_sw_backend->texture_rgba == NULL)
        return;
    sw = (int)ceilf(fabsf(source.width));
    sh = (int)ceilf(fabsf(source.height));
    if(sw <= 0 || sh <= 0 || (int)dest.width == 0 || (int)dest.height == 0)
        return;
    region = malloc((size_t)sw * sh * 4);
    if(region == NULL)
        return;
    for(y = 0; y < sh; y++) {
        int sy = (int)(source.y + y);

        for(x = 0; x < sw; x++) {
            int sx = (int)(source.x + x);
            unsigned char *dst = region + ((size_t)y * sw + x) * 4;

            if(sx < 0 || sy < 0 || sx >= t->width || sy >= t->height) {
                memset(dst, 0, 4);
            } else {
                const unsigned char *src = t->rgba + ((size_t)sy * t->width + sx) * 4;

                dst[0] = (unsigned char)((src[0] * (int)tint.r) / 255);
                dst[1] = (unsigned char)((src[1] * (int)tint.g) / 255);
                dst[2] = (unsigned char)((src[2] * (int)tint.b) / 255);
                dst[3] = (unsigned char)((src[3] * (int)tint.a) / 255);
            }
        }
    }
    g_sw_backend->texture_rgba(region, sw, sh, (int)dest.x, (int)dest.y,
                               (int)dest.width, (int)dest.height, 0xffffffffu);
    free(region);
}
void SetTextureFilter(Texture2D texture, int filter) { (void)texture; (void)filter; }
void SetShapesTexture(Texture2D texture, Rectangle source) { (void)texture; (void)source; }

static Image zero_image(void) { Image image; memset(&image, 0, sizeof(image)); return image; }
static Rectangle zero_rectangle(void) { Rectangle rec; memset(&rec, 0, sizeof(rec)); return rec; }
static GlyphInfo zero_glyph_info(void) { GlyphInfo glyph; memset(&glyph, 0, sizeof(glyph)); return glyph; }
static Font zero_font(void) { Font font; memset(&font, 0, sizeof(font)); return font; }

static Font
make_bitmap_font(void)
{
    enum { cell_w = 8, cell_h = 8, cols = 32, glyph_count = 95 };
    GlyphInfo *glyphs;
    Rectangle *recs;
    unsigned char *pixels;
    int rows = (glyph_count + cols - 1) / cols;
    int i;
    Font font;

    if(g_default_ready)
        return g_default_font;
    glyphs = calloc(glyph_count, sizeof(*glyphs));
    recs = calloc(glyph_count, sizeof(*recs));
    pixels = calloc((size_t)cols * cell_w * rows * cell_h * 4, 1);
    if(glyphs == NULL || recs == NULL || pixels == NULL) {
        free(glyphs);
        free(recs);
        free(pixels);
        return zero_font();
    }
    for(i = 0; i < glyph_count; i++) {
        int cp = 32 + i;
        int col = i % cols;
        int row = i / cols;
        int gy;

        glyphs[i].value = cp;
        glyphs[i].advanceX = cell_w;
        glyphs[i].image = zero_image();
        recs[i] = (Rectangle){(float)(col * cell_w), (float)(row * cell_h), cell_w, cell_h};
        for(gy = 0; gy < cell_h; gy++) {
            unsigned char bits = KrySwFont8x8[cp][gy];
            int gx;

            for(gx = 0; gx < cell_w; gx++) {
                unsigned char *dst;

                if(((bits >> gx) & 1) == 0)
                    continue;
                dst = pixels + ((size_t)(row * cell_h + gy) * (cols * cell_w) + col * cell_w + gx) * 4;
                dst[0] = 255;
                dst[1] = 255;
                dst[2] = 255;
                dst[3] = 255;
            }
        }
    }
    {
        Image image = {pixels, cols * cell_w, rows * cell_h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D tex = LoadTextureFromImage(image);

        font = (Font){0};
        font.baseSize = cell_h;
        font.glyphCount = glyph_count;
        font.glyphPadding = 0;
        font.texture = tex;
        font.recs = recs;
        font.glyphs = glyphs;
    }
    free(pixels);
    if(font.texture.id == 0) {
        free(glyphs);
        free(recs);
        return zero_font();
    }
    g_default_font = font;
    g_default_ready = 1;
    return g_default_font;
}

Font GetFontDefault(void) { return make_bitmap_font(); }
Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int fontSize, const int *codepoints, int codepointCount)
{
    (void)fileType;
    (void)fileData;
    (void)dataSize;
    (void)fontSize;
    (void)codepoints;
    (void)codepointCount;
    return make_bitmap_font();
}
Font LoadFontEx(const char *fileName, int fontSize, const int *codepoints, int codepointCount)
{
    (void)fileName;
    return LoadFontFromMemory(".ttf", NULL, 0, fontSize, codepoints, codepointCount);
}
Font LoadFont(const char *fileName) { return LoadFontEx(fileName, 16, NULL, 0); }
bool IsFontValid(Font font) { return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL && font.glyphCount > 0; }
void UnloadFont(Font font)
{
    Font def = GetFontDefault();

    if(font.texture.id != 0 && font.texture.id != def.texture.id)
        UnloadTexture(font.texture);
    if(font.texture.id != def.texture.id) {
        free(font.glyphs);
        free(font.recs);
    }
}
int GetGlyphIndex(Font font, int codepoint)
{
    int fallback = 0;
    int i;

    if(font.glyphs == NULL || font.glyphCount <= 0)
        return 0;
    for(i = 0; i < font.glyphCount; i++) {
        if(font.glyphs[i].value == '?')
            fallback = i;
        if(font.glyphs[i].value == codepoint)
            return i;
    }
    return fallback;
}
GlyphInfo GetGlyphInfo(Font font, int codepoint) { return IsFontValid(font) ? font.glyphs[GetGlyphIndex(font, codepoint)] : zero_glyph_info(); }
Rectangle GetGlyphAtlasRec(Font font, int codepoint) { return IsFontValid(font) ? font.recs[GetGlyphIndex(font, codepoint)] : zero_rectangle(); }
void DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize, Color tint)
{
    GlyphInfo glyph;
    Rectangle src;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font))
        return;
    glyph = GetGlyphInfo(font, codepoint);
    src = GetGlyphAtlasRec(font, codepoint);
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    DrawTexturePro(font.texture, src,
                   (Rectangle){position.x + glyph.offsetX * scale,
                               position.y + glyph.offsetY * scale,
                               src.width * scale, src.height * scale},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTextCodepoints(Font font, const int *codepoints, int count, Vector2 position, float fontSize, float spacing, Color tint)
{
    int i;
    Vector2 p = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; codepoints != NULL && i < count; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);

        DrawTextCodepoint(font, codepoints[i], p, fontSize, tint);
        p.x += glyph.advanceX * scale + spacing;
    }
}
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint)
{
    const char *p = text;
    Vector2 pen = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return;
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    while(*p != '\0') {
        int bytes = 0;
        int cp = GetCodepointNext(p, &bytes);
        GlyphInfo glyph;

        if(bytes <= 0)
            bytes = 1;
        if(cp == '\n') {
            pen.x = position.x;
            pen.y += fontSize;
            p += bytes;
            continue;
        }
        glyph = GetGlyphInfo(font, cp);
        DrawTextCodepoint(font, cp, pen, fontSize, tint);
        pen.x += glyph.advanceX * scale + spacing;
        p += bytes;
    }
}
void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
    DrawTextEx(GetFontDefault(), text, (Vector2){(float)posX, (float)posY}, (float)fontSize, 0.0f, color);
}
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing)
{
    const char *p = text;
    float scale;
    float x = 0.0f;
    float max_x = 0.0f;
    float lines = 1.0f;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return (Vector2){0, fontSize};
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    while(*p != '\0') {
        int bytes = 0;
        int cp = GetCodepointNext(p, &bytes);
        GlyphInfo glyph;

        if(bytes <= 0)
            bytes = 1;
        if(cp == '\n') {
            if(x > max_x)
                max_x = x;
            x = 0.0f;
            lines += 1.0f;
            p += bytes;
            continue;
        }
        glyph = GetGlyphInfo(font, cp);
        x += glyph.advanceX * scale + spacing;
        p += bytes;
    }
    if(x > max_x)
        max_x = x;
    return (Vector2){max_x, fontSize * lines};
}
int MeasureText(const char *text, int fontSize) { return (int)(MeasureTextEx(GetFontDefault(), text, (float)fontSize, 0).x + 0.5f); }
Vector2 MeasureTextCodepoints(Font font, const int *codepoints, int length, float fontSize, float spacing)
{
    int i;
    float scale;
    float width = 0.0f;

    if(!IsFontValid(font))
        font = GetFontDefault();
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; codepoints != NULL && i < length; i++)
        width += GetGlyphInfo(font, codepoints[i]).advanceX * scale + spacing;
    return (Vector2){width, fontSize};
}
GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize, int fontSize, const int *codepoints, int codepointCount, int type, int *glyphCount)
{
    Font font = LoadFontFromMemory(".ttf", fileData, dataSize, fontSize, codepoints, codepointCount);
    GlyphInfo *copy;

    (void)type;
    if(glyphCount != NULL)
        *glyphCount = 0;
    if(!IsFontValid(font))
        return NULL;
    copy = malloc((size_t)font.glyphCount * sizeof(*copy));
    if(copy != NULL) {
        memcpy(copy, font.glyphs, (size_t)font.glyphCount * sizeof(*copy));
        if(glyphCount != NULL)
            *glyphCount = font.glyphCount;
    }
    return copy;
}
void UnloadFontData(GlyphInfo *glyphs, int glyphCount) { (void)glyphCount; free(glyphs); }

bool KryonBackendRaw_IsKeyPressed(int key) { return key >= 0 && key < KRY_TUI_MAX_KEYS && g_key_pressed[key]; }
bool KryonBackendRaw_IsKeyPressedRepeat(int key) { return KryonBackendRaw_IsKeyPressed(key); }
bool KryonBackendRaw_IsKeyDown(int key) { return key >= 0 && key < KRY_TUI_MAX_KEYS && g_key_down[key]; }
bool KryonBackendRaw_IsKeyReleased(int key) { return key >= 0 && key < KRY_TUI_MAX_KEYS && g_key_released[key]; }
int KryonBackendRaw_GetKeyPressed(void) { return g_key_qr == g_key_qw ? 0 : g_key_queue[g_key_qr++ % KRY_TUI_KEY_QUEUE]; }
int KryonBackendRaw_GetCharPressed(void) { return g_char_qr == g_char_qw ? 0 : g_char_queue[g_char_qr++ % KRY_TUI_CHAR_QUEUE]; }
bool KryonBackendRaw_IsMouseButtonPressed(int button) { return button >= 0 && button < 3 && g_mouse_pressed[button]; }
bool KryonBackendRaw_IsMouseButtonDown(int button) { return button >= 0 && button < 3 && g_mouse_down[button]; }
bool KryonBackendRaw_IsMouseButtonReleased(int button) { return button >= 0 && button < 3 && g_mouse_released[button]; }
bool KryonBackendRaw_IsMouseButtonUp(int button) { return !KryonBackendRaw_IsMouseButtonDown(button); }
int KryonBackendRaw_GetMouseX(void) { return g_mouse_x; }
int KryonBackendRaw_GetMouseY(void) { return g_mouse_y; }
Vector2 KryonBackendRaw_GetMousePosition(void) { return (Vector2){(float)g_mouse_x, (float)g_mouse_y}; }
Vector2 KryonBackendRaw_GetMouseDelta(void) { return (Vector2){(float)g_mouse_dx, (float)g_mouse_dy}; }
float KryonBackendRaw_GetMouseWheelMove(void) { return (float)g_wheel; }
Vector2 KryonBackendRaw_GetMouseWheelMoveV(void) { return (Vector2){0.0f, (float)g_wheel}; }

void SetClipboardText(const char *text)
{
    char *copy;

    if(text == NULL)
        text = "";
    copy = malloc(strlen(text) + 1);
    if(copy == NULL)
        return;
    strcpy(copy, text);
    free(g_clipboard);
    g_clipboard = copy;
}
const char *GetClipboardText(void) { return g_clipboard != NULL ? g_clipboard : ""; }
bool FileExists(const char *fileName) { struct stat st; return fileName != NULL && stat(fileName, &st) == 0 && S_ISREG(st.st_mode); }
bool DirectoryExists(const char *dirPath) { struct stat st; return dirPath != NULL && stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode); }
bool IsFileExtension(const char *fileName, const char *ext)
{
    const char *dot = fileName != NULL ? strrchr(fileName, '.') : NULL;

    return dot != NULL && ext != NULL && strcmp(dot, ext) == 0;
}
const char *GetFileExtension(const char *fileName) { const char *dot = fileName != NULL ? strrchr(fileName, '.') : NULL; return dot != NULL ? dot : ""; }
const char *GetFileName(const char *filePath) { const char *slash = filePath != NULL ? strrchr(filePath, '/') : NULL; return slash != NULL ? slash + 1 : (filePath != NULL ? filePath : ""); }
const char *GetFileNameWithoutExt(const char *filePath)
{
    static char buf[1024];
    const char *name = GetFileName(filePath);
    const char *dot = strrchr(name, '.');
    size_t n = dot != NULL ? (size_t)(dot - name) : strlen(name);

    if(n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, name, n);
    buf[n] = '\0';
    return buf;
}
const char *GetDirectoryPath(const char *filePath)
{
    static char buf[1024];
    const char *slash;
    size_t n;

    if(filePath == NULL)
        return ".";
    slash = strrchr(filePath, '/');
    if(slash == NULL)
        return ".";
    n = (size_t)(slash - filePath);
    if(n == 0)
        n = 1;
    if(n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, filePath, n);
    buf[n] = '\0';
    return buf;
}
const char *GetWorkingDirectory(void) { static char buf[1024]; return getcwd(buf, sizeof(buf)) != NULL ? buf : ""; }
const char *GetApplicationDirectory(void) { return GetWorkingDirectory(); }
int MakeDirectory(const char *dirPath) { return dirPath != NULL && mkdir(dirPath, 0777) == 0 ? 0 : -1; }
int ChangeDirectory(const char *dirPath) { return dirPath != NULL && chdir(dirPath) == 0 ? 0 : -1; }
void OpenURL(const char *url) { (void)url; }
void TraceLog(int logLevel, const char *text, ...)
{
    va_list ap;

    (void)logLevel;
    if(text == NULL)
        return;
    va_start(ap, text);
    vfprintf(stderr, text, ap);
    fputc('\n', stderr);
    va_end(ap);
}
