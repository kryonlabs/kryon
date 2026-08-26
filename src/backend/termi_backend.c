#include "kryon.h"
#include "termi.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TERMI_KEY_CAP 512
#define TERMI_KEY_QUEUE_CAP 128
#define TERMI_CHAR_QUEUE_CAP 256
#define TERMI_CLIP_STACK_CAP 16
#define TERMI_MOUSE_QUEUE_CAP 128
#define TERMI_TEXTURE_CAP 512
#define TERMI_SIXEL_QUEUE_CAP 64
#define TERMI_SIXEL_PALETTE_CAP 64

typedef struct TermiClip {
    int x;
    int y;
    int w;
    int h;
} TermiClip;

typedef struct TermiMouseEvent {
    int button;
    int down;
    int x;
    int y;
    int wheel;
} TermiMouseEvent;

typedef struct TermiTexture {
    unsigned id;
    unsigned char *rgba;
    int width;
    int height;
    int format;
} TermiTexture;

typedef struct TermiSixelOp {
    unsigned texture_id;
    Rectangle source;
    Rectangle dest;
    Color tint;
} TermiSixelOp;

typedef struct TermiPaletteColor {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} TermiPaletteColor;

static TermiCell *g_cells;
static TermiCell *g_prev;
static int g_cols = 80;
static int g_rows = 24;
static int g_ready;
static int g_close_requested;
static int g_exit_key = KEY_ESCAPE;
static int g_target_fps;
static int g_trace_level = LOG_INFO;
static TraceLogCallback g_trace_callback;
static struct termios g_saved_termios;
static int g_have_termios;
static int g_saved_flags = -1;
static double g_last_time;
static float g_frame_time = 1.0f / 60.0f;
static int g_frame_counter;
static int g_fps;
static double g_fps_time;
static TermiClip g_clip_stack[TERMI_CLIP_STACK_CAP];
static int g_clip_depth;
static int g_key_pressed[TERMI_KEY_CAP];
static int g_key_down[TERMI_KEY_CAP];
static int g_key_released[TERMI_KEY_CAP];
static int g_key_queue[TERMI_KEY_QUEUE_CAP];
static int g_key_qr;
static int g_key_qw;
static int g_char_queue[TERMI_CHAR_QUEUE_CAP];
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
static TermiMouseEvent g_mouse_queue[TERMI_MOUSE_QUEUE_CAP];
static int g_mouse_qr;
static int g_mouse_qw;
static TermiTexture g_textures[TERMI_TEXTURE_CAP];
static unsigned g_next_texture_id = 16;
static TermiSixelOp g_sixel_queue[TERMI_SIXEL_QUEUE_CAP];
static TermiSixelOp g_prev_sixel_queue[TERMI_SIXEL_QUEUE_CAP];
static int g_sixel_count;
static int g_prev_sixel_count;
static int g_dirty_min_x;
static int g_dirty_min_y;
static int g_dirty_max_x;
static int g_dirty_max_y;

static GlyphInfo g_font_glyphs[96];
static Rectangle g_font_recs[96];
static Font g_default_font;
static int g_font_ready;

extern unsigned char *kry_decode_image_rgba(const unsigned char *data, int len,
                                            int *width, int *height);

static void present_sixel_queue(void);

static unsigned
pack_color(Color c)
{
    return ((unsigned)c.r << 24) | ((unsigned)c.g << 16) |
           ((unsigned)c.b << 8) | (unsigned)c.a;
}

static Color
unpack_color(unsigned rgba)
{
    Color c;

    c.r = (unsigned char)((rgba >> 24) & 0xff);
    c.g = (unsigned char)((rgba >> 16) & 0xff);
    c.b = (unsigned char)((rgba >> 8) & 0xff);
    c.a = (unsigned char)(rgba & 0xff);
    return c;
}

static double
now_seconds(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static int
env_int(const char *name, int fallback)
{
    const char *s = getenv(name);
    char *end = NULL;
    long v;

    if(s == NULL || s[0] == '\0')
        return fallback;
    v = strtol(s, &end, 10);
    if(end == s || v <= 0 || v > 10000)
        return fallback;
    return (int)v;
}

static int
env_enabled(const char *name, int fallback)
{
    const char *s = getenv(name);

    if(s == NULL || s[0] == '\0')
        return fallback;
    if(strcmp(s, "0") == 0 || strcmp(s, "false") == 0 ||
       strcmp(s, "FALSE") == 0 || strcmp(s, "no") == 0 ||
       strcmp(s, "NO") == 0)
        return 0;
    return 1;
}

static void
term_write(const char *s)
{
    if(s != NULL)
        (void)write(STDOUT_FILENO, s, strlen(s));
}

static void
term_write_len(const char *s, size_t len)
{
    if(s != NULL && len > 0)
        (void)write(STDOUT_FILENO, s, len);
}

static int
cell_index(int x, int y)
{
    return y * g_cols + x;
}

static int
pixel_to_col(int x)
{
    if(x <= 0)
        return 0;
    return x / TERMI_CELL_WIDTH;
}

static int
pixel_to_row(int y)
{
    if(y <= 0)
        return 0;
    return y / TERMI_CELL_HEIGHT;
}

static int
pixel_span_to_cols(int x, int w)
{
    int c0 = pixel_to_col(x);
    int c1;

    if(w <= 0)
        return 0;
    c1 = (x + w + TERMI_CELL_WIDTH - 1) / TERMI_CELL_WIDTH;
    if(c1 <= c0)
        c1 = c0 + 1;
    return c1 - c0;
}

static int
pixel_span_to_rows(int y, int h)
{
    int r0 = pixel_to_row(y);
    int r1;

    if(h <= 0)
        return 0;
    r1 = (y + h + TERMI_CELL_HEIGHT - 1) / TERMI_CELL_HEIGHT;
    if(r1 <= r0)
        r1 = r0 + 1;
    return r1 - r0;
}

static TermiClip
current_clip(void)
{
    if(g_clip_depth > 0)
        return g_clip_stack[g_clip_depth - 1];
    return (TermiClip){0, 0, g_cols, g_rows};
}

static int
inside_clip(int x, int y)
{
    TermiClip clip = current_clip();

    return x >= clip.x && y >= clip.y && x < clip.x + clip.w &&
           y < clip.y + clip.h && x >= 0 && y >= 0 && x < g_cols &&
           y < g_rows;
}

static void
cell_set(int x, int y, const char *text, unsigned fg, unsigned bg,
         unsigned attr)
{
    TermiCell *cell;

    if(!inside_clip(x, y) || g_cells == NULL)
        return;
    cell = &g_cells[cell_index(x, y)];
    if((bg & 0xffu) == 0)
        bg = cell->bg;
    memset(cell->text, 0, sizeof(cell->text));
    if(text == NULL || text[0] == '\0')
        cell->text[0] = ' ';
    else
        snprintf(cell->text, sizeof(cell->text), "%s", text);
    cell->fg = fg;
    cell->bg = bg;
    cell->attr = attr;
}

static void
resize_grid(int cols, int rows)
{
    size_t count;
    TermiCell *cells;
    TermiCell *prev;

    if(cols <= 0)
        cols = 80;
    if(rows <= 0)
        rows = 24;
    if(cols == g_cols && rows == g_rows && g_cells != NULL && g_prev != NULL)
        return;
    count = (size_t)cols * (size_t)rows;
    cells = calloc(count, sizeof(TermiCell));
    prev = calloc(count, sizeof(TermiCell));
    if(cells == NULL || prev == NULL) {
        free(cells);
        free(prev);
        return;
    }
    free(g_cells);
    free(g_prev);
    g_cols = cols;
    g_rows = rows;
    g_cells = cells;
    g_prev = prev;
    memset(g_prev, 0xff, count * sizeof(TermiCell));
    g_clip_depth = 0;
}

static void
detect_size(void)
{
    struct winsize ws;
    int cols = env_int("TERMI_COLS", 0);
    int rows = env_int("TERMI_ROWS", 0);

    if(cols <= 0 || rows <= 0) {
        if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if(cols <= 0 && ws.ws_col > 0)
                cols = ws.ws_col;
            if(rows <= 0 && ws.ws_row > 0)
                rows = ws.ws_row;
        }
    }
    if(cols <= 0)
        cols = env_int("COLUMNS", 80);
    if(rows <= 0)
        rows = env_int("LINES", 24);
    resize_grid(cols, rows);
}

static void
clear_input_edges(void)
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
queue_key(int key)
{
    if(key <= 0 || key >= TERMI_KEY_CAP)
        return;
    g_key_pressed[key] = 1;
    g_key_down[key] = 1;
    g_key_queue[g_key_qw++ % TERMI_KEY_QUEUE_CAP] = key;
    if(key == g_exit_key)
        g_close_requested = 1;
}

static void
queue_char(int ch)
{
    if(ch <= 0)
        return;
    g_char_queue[g_char_qw++ % TERMI_CHAR_QUEUE_CAP] = ch;
}

static void
queue_mouse_event(int button, int down, int x, int y, int wheel)
{
    TermiMouseEvent event;

    if(g_mouse_qw - g_mouse_qr >= TERMI_MOUSE_QUEUE_CAP)
        g_mouse_qr++;
    event.button = button;
    event.down = down;
    event.x = x;
    event.y = y;
    event.wheel = wheel;
    g_mouse_queue[g_mouse_qw++ % TERMI_MOUSE_QUEUE_CAP] = event;
}

static void
apply_next_mouse_event(void)
{
    TermiMouseEvent event;
    int old_x;
    int old_y;

    if(g_mouse_qr == g_mouse_qw)
        return;
    event = g_mouse_queue[g_mouse_qr++ % TERMI_MOUSE_QUEUE_CAP];
    old_x = g_mouse_x;
    old_y = g_mouse_y;
    g_mouse_x = event.x;
    g_mouse_y = event.y;
    g_mouse_dx += g_mouse_x - old_x;
    g_mouse_dy += g_mouse_y - old_y;
    if(event.wheel != 0) {
        g_wheel += event.wheel;
        return;
    }
    if(event.button < 0 || event.button >= 3)
        return;
    if(event.down) {
        if(!g_mouse_down[event.button])
            g_mouse_pressed[event.button] = 1;
        g_mouse_down[event.button] = 1;
        return;
    }
    if(g_mouse_down[event.button])
        g_mouse_released[event.button] = 1;
    g_mouse_down[event.button] = 0;
}

static int
key_from_ascii(unsigned char c)
{
    if(c >= 'a' && c <= 'z')
        return KEY_A + (c - 'a');
    if(c >= 'A' && c <= 'Z')
        return KEY_A + (c - 'A');
    if(c >= '0' && c <= '9')
        return KEY_ZERO + (c - '0');
    switch(c) {
    case ' ': return KEY_SPACE;
    case '\t': return KEY_TAB;
    case '\r':
    case '\n': return KEY_ENTER;
    case 127:
    case '\b': return KEY_BACKSPACE;
    case 27: return KEY_ESCAPE;
    case '-': return KEY_MINUS;
    case '=': return KEY_EQUAL;
    case '[': return KEY_LEFT_BRACKET;
    case ']': return KEY_RIGHT_BRACKET;
    case '\\': return KEY_BACKSLASH;
    case ';': return KEY_SEMICOLON;
    case '\'': return KEY_APOSTROPHE;
    case ',': return KEY_COMMA;
    case '.': return KEY_PERIOD;
    case '/': return KEY_SLASH;
    case '`': return KEY_GRAVE;
    default: return 0;
    }
}

static void
handle_mouse_sgr(const char *buf, int len)
{
    int b = 0;
    int x = 0;
    int y = 0;
    char kind = 0;
    int button = 0;
    int px;
    int py;

    if(len < 6)
        return;
    if(sscanf(buf, "\033[<%d;%d;%d%c", &b, &x, &y, &kind) != 4)
        return;
    px = x > 0 ? (x - 1) * TERMI_CELL_WIDTH : g_mouse_x;
    py = y > 0 ? (y - 1) * TERMI_CELL_HEIGHT : g_mouse_y;
    if((b & 64) != 0) {
        queue_mouse_event(-1, 0, px, py, (b & 1) ? -1 : 1);
        return;
    }
    if((b & 3) == 1)
        button = MOUSE_BUTTON_MIDDLE;
    else if((b & 3) == 2)
        button = MOUSE_BUTTON_RIGHT;
    else
        button = MOUSE_BUTTON_LEFT;
    if(button < 0 || button >= 3)
        return;
    if(kind == 'm')
        queue_mouse_event(button, 0, px, py, 0);
    else if(kind == 'M')
        queue_mouse_event(button, 1, px, py, 0);
}

static void
parse_escape_sequence(const char *buf, int len)
{
    char final;

    if(len < 2)
        return;
    if(len >= 6 && buf[1] == '[' && buf[2] == '<') {
        handle_mouse_sgr(buf, len);
        return;
    }
    final = buf[len - 1];
    switch(final) {
    case 'A': queue_key(KEY_UP); break;
    case 'B': queue_key(KEY_DOWN); break;
    case 'C': queue_key(KEY_RIGHT); break;
    case 'D': queue_key(KEY_LEFT); break;
    case 'H': queue_key(KEY_HOME); break;
    case 'F': queue_key(KEY_END); break;
    case 'Z':
        queue_key(KEY_LEFT_SHIFT);
        queue_key(KEY_TAB);
        break;
    case '~':
        if(strstr(buf, "[1~") != NULL || strstr(buf, "[7~") != NULL)
            queue_key(KEY_HOME);
        else if(strstr(buf, "[4~") != NULL || strstr(buf, "[8~") != NULL)
            queue_key(KEY_END);
        else if(strstr(buf, "[3~") != NULL)
            queue_key(KEY_DELETE);
        else if(strstr(buf, "[5~") != NULL)
            queue_key(KEY_PAGE_UP);
        else if(strstr(buf, "[6~") != NULL)
            queue_key(KEY_PAGE_DOWN);
        break;
    default:
        queue_key(KEY_ESCAPE);
        break;
    }
}

static void
poll_input(int reset_edges)
{
    unsigned char buf[128];
    ssize_t n;
    int i = 0;

    if(reset_edges)
        clear_input_edges();
    for(;;) {
        n = read(STDIN_FILENO, buf, sizeof(buf));
        if(n <= 0) {
            if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                g_close_requested = 1;
            break;
        }
        i = 0;
        while(i < n) {
            unsigned char c = buf[i++];
            int key;

            if(c == 3) {
                g_close_requested = 1;
                queue_key(KEY_C);
                continue;
            }
            if(c == 27 && i < n && buf[i] == '[') {
                int start = i - 1;
                int end = i + 1;

                while(end < n && end - start < 63) {
                    unsigned char f = buf[end++];
                    if((f >= '@' && f <= '~') || f == 'M' || f == 'm')
                        break;
                }
                parse_escape_sequence((const char *)&buf[start], end - start);
                i = end;
                continue;
            }
            key = key_from_ascii(c);
            if(key != 0)
                queue_key(key);
            if(c >= 32 && c < 127)
                queue_char((int)c);
        }
    }
    if(reset_edges)
        apply_next_mouse_event();
}

static void
ensure_font(void)
{
    if(g_font_ready)
        return;
    memset(g_font_glyphs, 0, sizeof(g_font_glyphs));
    memset(g_font_recs, 0, sizeof(g_font_recs));
    for(int i = 0; i < 96; i++) {
        g_font_glyphs[i].value = 32 + i;
        g_font_glyphs[i].advanceX = TERMI_CELL_WIDTH;
        g_font_recs[i] = (Rectangle){0.0f, 0.0f, (float)TERMI_CELL_WIDTH,
                                     (float)TERMI_CELL_HEIGHT};
    }
    g_default_font.baseSize = TERMI_CELL_HEIGHT;
    g_default_font.glyphCount = 96;
    g_default_font.glyphPadding = 0;
    g_default_font.texture.id = 1;
    g_default_font.texture.width = TERMI_CELL_WIDTH;
    g_default_font.texture.height = TERMI_CELL_HEIGHT;
    g_default_font.texture.mipmaps = 1;
    g_default_font.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    g_default_font.recs = g_font_recs;
    g_default_font.glyphs = g_font_glyphs;
    g_font_ready = 1;
}

static int
utf8_next_width(const char *s, int *bytes)
{
    unsigned char c = (unsigned char)s[0];

    if(c == '\0') {
        *bytes = 0;
        return 0;
    }
    if(c < 0x80) {
        *bytes = 1;
        return c == '\t' ? 4 : 1;
    }
    if((c & 0xe0) == 0xc0)
        *bytes = 2;
    else if((c & 0xf0) == 0xe0)
        *bytes = 3;
    else if((c & 0xf8) == 0xf0)
        *bytes = 4;
    else
        *bytes = 1;
    return 1;
}

static int
text_cells(const char *text, int byte_len)
{
    int w = 0;
    int i = 0;

    if(text == NULL)
        return 0;
    while(text[i] != '\0' && (byte_len < 0 || i < byte_len)) {
        int bytes = 0;
        if(text[i] == '\n')
            break;
        w += utf8_next_width(&text[i], &bytes);
        if(bytes <= 0)
            break;
        i += bytes;
    }
    return w;
}

static void
draw_text_cells(const char *text, int byte_len, int col, int row, unsigned fg,
                unsigned bg, unsigned attr)
{
    int i = 0;
    int x = col;

    if(text == NULL)
        return;
    while(text[i] != '\0' && (byte_len < 0 || i < byte_len)) {
        int bytes = 0;
        char glyph[5] = {0, 0, 0, 0, 0};

        if(text[i] == '\n')
            break;
        (void)utf8_next_width(&text[i], &bytes);
        if(bytes <= 0)
            break;
        if(text[i] == '\t') {
            for(int t = 0; t < 4; t++)
                cell_set(x++, row, " ", fg, bg, attr);
            i += bytes;
            continue;
        }
        if(bytes > 4)
            bytes = 4;
        memcpy(glyph, &text[i], (size_t)bytes);
        cell_set(x++, row, glyph, fg, bg, attr);
        i += bytes;
    }
}

static void
ansi_color(char *out, size_t out_size, int is_bg, unsigned rgba)
{
    Color c = unpack_color(rgba);

    snprintf(out, out_size, "\033[%d;2;%u;%u;%um", is_bg ? 48 : 38,
             (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
}

static void
ansi_attr(char *out, size_t out_size, unsigned attr)
{
    char buf[64] = "";
    int first = 1;

    snprintf(buf, sizeof(buf), "\033[0");
    if((attr & TERMI_ATTR_BOLD) != 0) {
        strncat(buf, ";1", sizeof(buf) - strlen(buf) - 1);
        first = 0;
    }
    if((attr & TERMI_ATTR_DIM) != 0) {
        strncat(buf, ";2", sizeof(buf) - strlen(buf) - 1);
        first = 0;
    }
    if((attr & TERMI_ATTR_UNDERLINE) != 0) {
        strncat(buf, ";4", sizeof(buf) - strlen(buf) - 1);
        first = 0;
    }
    if((attr & TERMI_ATTR_REVERSE) != 0) {
        strncat(buf, ";7", sizeof(buf) - strlen(buf) - 1);
        first = 0;
    }
    (void)first;
    strncat(buf, "m", sizeof(buf) - strlen(buf) - 1);
    snprintf(out, out_size, "%s", buf);
}

static int
cell_equal(const TermiCell *a, const TermiCell *b)
{
    return strcmp(a->text, b->text) == 0 && a->fg == b->fg && a->bg == b->bg &&
           a->attr == b->attr;
}

static void
dirty_reset(void)
{
    g_dirty_min_x = g_cols;
    g_dirty_min_y = g_rows;
    g_dirty_max_x = -1;
    g_dirty_max_y = -1;
}

static void
dirty_mark(int x, int y)
{
    if(x < g_dirty_min_x)
        g_dirty_min_x = x;
    if(y < g_dirty_min_y)
        g_dirty_min_y = y;
    if(x > g_dirty_max_x)
        g_dirty_max_x = x;
    if(y > g_dirty_max_y)
        g_dirty_max_y = y;
}

static int
dirty_empty(void)
{
    return g_dirty_max_x < g_dirty_min_x || g_dirty_max_y < g_dirty_min_y;
}

static void
sleep_seconds(double seconds)
{
    if(seconds <= 0.0)
        return;
    usleep((useconds_t)(seconds * 1000000.0));
}

int termi_cols(void) { return g_cols; }
int termi_rows(void) { return g_rows; }
int termi_cell_width(void) { return TERMI_CELL_WIDTH; }
int termi_cell_height(void) { return TERMI_CELL_HEIGHT; }

void
termi_clear(unsigned bg_rgba)
{
    unsigned fg = 0xffffffffu;

    if(g_cells == NULL)
        return;
    for(int y = 0; y < g_rows; y++) {
        for(int x = 0; x < g_cols; x++)
            cell_set(x, y, " ", fg, bg_rgba, TERMI_ATTR_NONE);
    }
}

void
termi_rect(int x, int y, int w, int h, unsigned bg_rgba)
{
    int col = pixel_to_col(x);
    int row = pixel_to_row(y);
    int cols = pixel_span_to_cols(x, w);
    int rows = pixel_span_to_rows(y, h);

    if(((bg_rgba) & 0xffu) == 0)
        return;
    for(int yy = 0; yy < rows; yy++) {
        for(int xx = 0; xx < cols; xx++)
            cell_set(col + xx, row + yy, " ", 0xffffffffu, bg_rgba,
                     TERMI_ATTR_NONE);
    }
}

void
termi_text(const char *text, int x, int y, unsigned fg_rgba, unsigned bg_rgba,
           unsigned attr)
{
    if(((fg_rgba) & 0xffu) == 0)
        return;
    draw_text_cells(text, -1, pixel_to_col(x), pixel_to_row(y), fg_rgba,
                    bg_rgba, attr);
}

void
termi_present(void)
{
    char seq[128];
    unsigned last_fg = 0;
    unsigned last_bg = 0;
    unsigned last_attr = 0xffffffffu;
    size_t count;

    if(g_cells == NULL || g_prev == NULL)
        return;
    dirty_reset();
    for(int y = 0; y < g_rows; y++) {
        int x = 0;

        while(x < g_cols) {
            TermiCell *cell = &g_cells[cell_index(x, y)];
            TermiCell *prev = &g_prev[cell_index(x, y)];

            if(cell_equal(cell, prev)) {
                x++;
                continue;
            }
            snprintf(seq, sizeof(seq), "\033[%d;%dH", y + 1, x + 1);
            term_write(seq);
            while(x < g_cols) {
                cell = &g_cells[cell_index(x, y)];
                prev = &g_prev[cell_index(x, y)];
                if(cell_equal(cell, prev))
                    break;
                if(cell->attr != last_attr) {
                    ansi_attr(seq, sizeof(seq), cell->attr);
                    term_write(seq);
                    last_attr = cell->attr;
                    last_fg = 0;
                    last_bg = 0;
                }
                if(cell->fg != last_fg) {
                    ansi_color(seq, sizeof(seq), 0, cell->fg);
                    term_write(seq);
                    last_fg = cell->fg;
                }
                if(cell->bg != last_bg) {
                    ansi_color(seq, sizeof(seq), 1, cell->bg);
                    term_write(seq);
                    last_bg = cell->bg;
                }
                term_write(cell->text[0] != '\0' ? cell->text : " ");
                *prev = *cell;
                dirty_mark(x, y);
                x++;
            }
        }
    }
    term_write("\033[0m\033[?25l");
    count = (size_t)g_cols * (size_t)g_rows;
    (void)count;
    fflush(stdout);
}

void termi_request_close(void) { g_close_requested = 1; }

int
termi_font_height(unsigned id)
{
    return id != 0 ? TERMI_CELL_HEIGHT : 0;
}

int
termi_text_width(unsigned id, const char *text, int byte_len)
{
    (void)id;
    return text_cells(text, byte_len) * TERMI_CELL_WIDTH;
}

void
termi_queue_text(unsigned font_id, const char *text, int byte_len, int x, int y,
                 int font_size, unsigned rgba)
{
    unsigned attr = font_size >= 20 ? TERMI_ATTR_BOLD : TERMI_ATTR_NONE;

    (void)font_id;
    draw_text_cells(text, byte_len, pixel_to_col(x), pixel_to_row(y), rgba,
                    0x00000000u, attr);
}

void
KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
    detect_size();
    ensure_font();
    if(isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &g_saved_termios) == 0) {
        struct termios raw = g_saved_termios;

        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
        raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0)
            g_have_termios = 1;
    }
    g_saved_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if(g_saved_flags >= 0)
        (void)fcntl(STDIN_FILENO, F_SETFL, g_saved_flags | O_NONBLOCK);
    term_write("\033[?1049h\033[?25l\033[?1000h\033[?1006h\033[2J");
    g_ready = 1;
    g_close_requested = 0;
    g_last_time = now_seconds();
    g_fps_time = g_last_time;
    termi_clear(0x000000ffu);
    termi_present();
}

void
KryonRaylibBackend_CloseWindow(void)
{
    if(!g_ready)
        return;
    term_write("\033[0m\033[?1006l\033[?1000l\033[?25h\033[?1049l");
    if(g_have_termios) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_termios);
        g_have_termios = 0;
    }
    if(g_saved_flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, g_saved_flags);
        g_saved_flags = -1;
    }
    g_ready = 0;
}

bool
KryonRaylibBackend_WindowShouldClose(void)
{
    poll_input(0);
    return g_close_requested != 0;
}

void
KryonRaylibBackend_EndDrawing(void)
{
    double now;
    double elapsed;

    termi_present();
    present_sixel_queue();
    now = now_seconds();
    if(g_target_fps > 0 && g_last_time > 0.0) {
        double target = 1.0 / (double)g_target_fps;

        elapsed = now - g_last_time;
        if(elapsed < target) {
            sleep_seconds(target - elapsed);
            now = now_seconds();
        }
    }
    g_frame_time = (float)(now - g_last_time);
    if(g_frame_time <= 0.0f)
        g_frame_time = 1.0f / 60.0f;
    g_last_time = now;
    g_frame_counter++;
    if(now - g_fps_time >= 1.0) {
        g_fps = g_frame_counter;
        g_frame_counter = 0;
        g_fps_time = now;
    }
}

bool IsWindowReady(void) { return g_ready != 0; }
bool IsWindowFocused(void) { return g_ready != 0; }
bool IsWindowFullscreen(void) { return false; }
bool IsWindowHidden(void) { return false; }
bool IsWindowMinimized(void) { return false; }
bool IsWindowMaximized(void) { return false; }
bool IsWindowResized(void) { return false; }
bool IsWindowState(unsigned int flag) { (void)flag; return false; }
void SetWindowState(unsigned int flags) { (void)flags; }
void ClearWindowState(unsigned int flags) { (void)flags; }
void ToggleFullscreen(void) {}
void ToggleBorderlessWindowed(void) {}
void MaximizeWindow(void) {}
void MinimizeWindow(void) {}
void RestoreWindow(void) {}
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }
void SetWindowTitle(const char *title) { (void)title; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height) { (void)width; (void)height; }
void SetWindowMaxSize(int width, int height) { (void)width; (void)height; }
void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) {}
void *GetWindowHandle(void) { return NULL; }
void SetConfigFlags(unsigned int flags) { (void)flags; }
void SetTargetFPS(int fps) { g_target_fps = fps > 0 ? fps : 0; }
void SetExitKey(int key) { g_exit_key = key; }
void SetMouseCursor(int cursor) { (void)cursor; }
void SetWindowSize(int width, int height)
{
    int cols = (width + TERMI_CELL_WIDTH - 1) / TERMI_CELL_WIDTH;
    int rows = (height + TERMI_CELL_HEIGHT - 1) / TERMI_CELL_HEIGHT;

    resize_grid(cols, rows);
}
int GetScreenWidth(void) { return g_cols * TERMI_CELL_WIDTH; }
int GetScreenHeight(void) { return g_rows * TERMI_CELL_HEIGHT; }
int GetRenderWidth(void) { return GetScreenWidth(); }
int GetRenderHeight(void) { return GetScreenHeight(); }
int GetCurrentMonitor(void) { return 0; }
int GetMonitorCount(void) { return 1; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){0}; }
int GetMonitorWidth(int monitor) { (void)monitor; return GetScreenWidth(); }
int GetMonitorHeight(int monitor) { (void)monitor; return GetScreenHeight(); }
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return GetScreenWidth(); }
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return GetScreenHeight(); }
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 60; }
Vector2 GetWindowPosition(void) { return (Vector2){0}; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){1.0f, 1.0f}; }
const char *GetMonitorName(int monitor) { (void)monitor; return "terminal"; }
float GetFrameTime(void) { return g_frame_time; }
double GetTime(void) { return now_seconds(); }
int GetFPS(void) { return g_fps > 0 ? g_fps : 60; }
void WaitTime(double seconds) { sleep_seconds(seconds); }

void
BeginDrawing(void)
{
    detect_size();
    poll_input(1);
    g_sixel_count = 0;
}

void ClearBackground(Color color) { termi_clear(pack_color(color)); }
void DrawPixel(int posX, int posY, Color color)
{
    cell_set(pixel_to_col(posX), pixel_to_row(posY), " ", 0xffffffffu,
             pack_color(color), TERMI_ATTR_NONE);
}
void DrawPixelV(Vector2 position, Color color)
{
    DrawPixel((int)position.x, (int)position.y, color);
}
void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    termi_rect(posX, posY, width, height, pack_color(color));
}
void DrawRectangleV(Vector2 position, Vector2 size, Color color)
{
    DrawRectangle((int)position.x, (int)position.y, (int)size.x, (int)size.y,
                  color);
}
void DrawRectangleRec(Rectangle rec, Color color)
{
    DrawRectangle((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height,
                  color);
}
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color)
{
    (void)origin;
    (void)rotation;
    DrawRectangleRec(rec, color);
}
void DrawRectangleGradientV(int posX, int posY, int width, int height,
                            Color top, Color bottom)
{
    (void)bottom;
    DrawRectangle(posX, posY, width, height, top);
}
void DrawRectangleGradientH(int posX, int posY, int width, int height,
                            Color left, Color right)
{
    (void)right;
    DrawRectangle(posX, posY, width, height, left);
}
void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3,
                             Color col4)
{
    (void)col2;
    (void)col3;
    (void)col4;
    DrawRectangleRec(rec, col1);
}
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
    DrawRectangleLinesEx((Rectangle){(float)posX, (float)posY, (float)width,
                                     (float)height},
                         1.0f, color);
}
void DrawRectangleLinesEx(Rectangle rec, float thick, Color color)
{
    int c0 = pixel_to_col((int)rec.x);
    int r0 = pixel_to_row((int)rec.y);
    int cols = pixel_span_to_cols((int)rec.x, (int)rec.width);
    int rows = pixel_span_to_rows((int)rec.y, (int)rec.height);
    unsigned fg = pack_color(color);
    int t = (int)(thick + 0.5f);

    if(t < 1)
        t = 1;
    if(cols <= 0 || rows <= 0)
        return;
    if(t > cols)
        t = cols;
    if(t > rows)
        t = rows;
    for(int layer = 0; layer < t; layer++) {
        int left = c0 + layer;
        int top = r0 + layer;
        int right = c0 + cols - 1 - layer;
        int bottom = r0 + rows - 1 - layer;

        if(left > right || top > bottom)
            break;
        for(int x = left; x <= right; x++) {
            cell_set(x, top, "-", fg, 0x00000000u, TERMI_ATTR_NONE);
            cell_set(x, bottom, "-", fg, 0x00000000u, TERMI_ATTR_NONE);
        }
        for(int y = top; y <= bottom; y++) {
            cell_set(left, y, "|", fg, 0x00000000u, TERMI_ATTR_NONE);
            cell_set(right, y, "|", fg, 0x00000000u, TERMI_ATTR_NONE);
        }
        cell_set(left, top, "+", fg, 0x00000000u, TERMI_ATTR_NONE);
        cell_set(right, top, "+", fg, 0x00000000u, TERMI_ATTR_NONE);
        cell_set(left, bottom, "+", fg, 0x00000000u, TERMI_ATTR_NONE);
        cell_set(right, bottom, "+", fg, 0x00000000u, TERMI_ATTR_NONE);
    }
}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                          Color color)
{
    (void)roundness;
    (void)segments;
    DrawRectangleRec(rec, color);
}
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                               Color color)
{
    DrawRectangleRoundedLinesEx(rec, roundness, segments, 1.0f, color);
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                 float thick, Color color)
{
    (void)roundness;
    (void)segments;
    DrawRectangleLinesEx(rec, thick, color);
}
void DrawLine(int x1, int y1, int x2, int y2, Color color)
{
    int c0 = pixel_to_col(x1);
    int r0 = pixel_to_row(y1);
    int c1 = pixel_to_col(x2);
    int r1 = pixel_to_row(y2);
    int dx = abs(c1 - c0);
    int sx = c0 < c1 ? 1 : -1;
    int dy = -abs(r1 - r0);
    int sy = r0 < r1 ? 1 : -1;
    int err = dx + dy;
    unsigned fg = pack_color(color);

    for(;;) {
        cell_set(c0, r0, dx > -dy ? "-" : "|", fg, 0x00000000u,
                 TERMI_ATTR_NONE);
        if(c0 == c1 && r0 == r1)
            break;
        if(2 * err >= dy) {
            err += dy;
            c0 += sx;
        }
        if(2 * err <= dx) {
            err += dx;
            r0 += sy;
        }
    }
}
void DrawLineV(Vector2 start, Vector2 end, Color color)
{
    DrawLine((int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
}
void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color)
{
    (void)thick;
    DrawLineV(start, end, color);
}
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    int x = centerX - (int)radius;
    int y = centerY - (int)radius;
    int d = (int)(radius * 2.0f);

    DrawRectangle(x, y, d, d, color);
}
void DrawCircleV(Vector2 center, float radius, Color color)
{
    DrawCircle((int)center.x, (int)center.y, radius, color);
}
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    DrawRectangleLines(centerX - (int)radius, centerY - (int)radius,
                       (int)(radius * 2.0f), (int)(radius * 2.0f), color);
}
void DrawCircleLinesV(Vector2 center, float radius, Color color)
{
    DrawCircleLines((int)center.x, (int)center.y, radius, color);
}
void DrawCircleLinesEx(Vector2 center, float radius, float thick, Color color)
{
    (void)thick;
    DrawCircleLinesV(center, radius, color);
}
void DrawRing(Vector2 center, float innerRadius, float outerRadius,
              float startAngle, float endAngle, int segments, Color color)
{
    (void)innerRadius;
    (void)startAngle;
    (void)endAngle;
    (void)segments;
    DrawCircleLinesV(center, outerRadius, color);
}
void DrawRingLines(Vector2 center, float innerRadius, float outerRadius,
                   float startAngle, float endAngle, int segments, Color color)
{
    DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments,
             color);
}
void BeginScissorMode(int x, int y, int width, int height)
{
    TermiClip clip = {pixel_to_col(x), pixel_to_row(y),
                      pixel_span_to_cols(x, width),
                      pixel_span_to_rows(y, height)};
    TermiClip parent = current_clip();
    int x0;
    int y0;
    int x1;
    int y1;

    x0 = clip.x > parent.x ? clip.x : parent.x;
    y0 = clip.y > parent.y ? clip.y : parent.y;
    x1 = clip.x + clip.w < parent.x + parent.w ? clip.x + clip.w
                                                : parent.x + parent.w;
    y1 = clip.y + clip.h < parent.y + parent.h ? clip.y + clip.h
                                                : parent.y + parent.h;
    clip.x = x0;
    clip.y = y0;
    clip.w = x1 > x0 ? x1 - x0 : 0;
    clip.h = y1 > y0 ? y1 - y0 : 0;
    if(g_clip_depth < TERMI_CLIP_STACK_CAP)
        g_clip_stack[g_clip_depth++] = clip;
}
void EndScissorMode(void)
{
    if(g_clip_depth > 0)
        g_clip_depth--;
}
void BeginMode2D(Camera2D camera) { (void)camera; }
void EndMode2D(void) {}

bool KryonBackendRaw_IsKeyPressed(int key)
{
    return key >= 0 && key < TERMI_KEY_CAP && g_key_pressed[key];
}
bool KryonBackendRaw_IsKeyPressedRepeat(int key)
{
    return KryonBackendRaw_IsKeyPressed(key);
}
bool KryonBackendRaw_IsKeyDown(int key)
{
    return key >= 0 && key < TERMI_KEY_CAP && g_key_down[key];
}
bool KryonBackendRaw_IsKeyReleased(int key)
{
    return key >= 0 && key < TERMI_KEY_CAP && g_key_released[key];
}
int KryonBackendRaw_GetKeyPressed(void)
{
    if(g_key_qr == g_key_qw)
        return 0;
    return g_key_queue[g_key_qr++ % TERMI_KEY_QUEUE_CAP];
}
int KryonBackendRaw_GetCharPressed(void)
{
    if(g_char_qr == g_char_qw)
        return 0;
    return g_char_queue[g_char_qr++ % TERMI_CHAR_QUEUE_CAP];
}
bool KryonBackendRaw_IsMouseButtonPressed(int button)
{
    return button >= 0 && button < 3 && g_mouse_pressed[button];
}
bool KryonBackendRaw_IsMouseButtonDown(int button)
{
    return button >= 0 && button < 3 && g_mouse_down[button];
}
bool KryonBackendRaw_IsMouseButtonReleased(int button)
{
    return button >= 0 && button < 3 && g_mouse_released[button];
}
bool KryonBackendRaw_IsMouseButtonUp(int button)
{
    return !KryonBackendRaw_IsMouseButtonDown(button);
}
int KryonBackendRaw_GetMouseX(void) { return g_mouse_x; }
int KryonBackendRaw_GetMouseY(void) { return g_mouse_y; }
Vector2 KryonBackendRaw_GetMousePosition(void)
{
    return (Vector2){(float)g_mouse_x, (float)g_mouse_y};
}
Vector2 KryonBackendRaw_GetMouseDelta(void)
{
    return (Vector2){(float)g_mouse_dx, (float)g_mouse_dy};
}
float KryonBackendRaw_GetMouseWheelMove(void) { return (float)g_wheel; }
Vector2 KryonBackendRaw_GetMouseWheelMoveV(void)
{
    return (Vector2){0.0f, (float)g_wheel};
}

Font GetFontDefault(void)
{
    ensure_font();
    return g_default_font;
}
Font LoadFont(const char *fileName)
{
    (void)fileName;
    return GetFontDefault();
}
Font LoadFontEx(const char *fileName, int fontSize, const int *codepoints,
                int codepointCount)
{
    (void)fileName;
    (void)fontSize;
    (void)codepoints;
    (void)codepointCount;
    return GetFontDefault();
}
Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize, int fontSize, const int *codepoints,
                        int codepointCount)
{
    Font font;

    (void)fileType;
    (void)fileData;
    (void)dataSize;
    (void)codepoints;
    (void)codepointCount;
    font = GetFontDefault();
    if(fontSize > 0)
        font.baseSize = fontSize;
    return font;
}
bool IsFontValid(Font font) { return font.texture.id != 0; }
void UnloadFont(Font font) { (void)font; }
void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
    termi_queue_text(1, text, -1, posX, posY, fontSize, pack_color(color));
}
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize,
                float spacing, Color tint)
{
    (void)spacing;
    termi_queue_text(font.texture.id, text, -1, (int)position.x,
                     (int)position.y, (int)fontSize, pack_color(tint));
}
void DrawTextCodepoint(Font font, int codepoint, Vector2 position,
                       float fontSize, Color tint)
{
    char text[2] = {(char)(codepoint >= 32 && codepoint < 127 ? codepoint : '?'),
                   '\0'};

    DrawTextEx(font, text, position, fontSize, 0.0f, tint);
}
void DrawTextCodepoints(Font font, const int *codepoints, int codepointCount,
                        Vector2 position, float fontSize, float spacing,
                        Color tint)
{
    int x = (int)position.x;

    for(int i = 0; i < codepointCount; i++) {
        DrawTextCodepoint(font, codepoints[i], (Vector2){(float)x, position.y},
                          fontSize, tint);
        x += TERMI_CELL_WIDTH + (int)spacing;
    }
}
int MeasureText(const char *text, int fontSize)
{
    (void)fontSize;
    return text_cells(text, -1) * TERMI_CELL_WIDTH;
}
Vector2 MeasureTextEx(Font font, const char *text, float fontSize,
                      float spacing)
{
    int cells;

    (void)font;
    (void)spacing;
    cells = text_cells(text, -1);
    return (Vector2){(float)(cells * TERMI_CELL_WIDTH), fontSize};
}

static TermiTexture *
texture_find(unsigned id)
{
    if(id == 0)
        return NULL;
    for(int i = 0; i < TERMI_TEXTURE_CAP; i++) {
        if(g_textures[i].id == id)
            return &g_textures[i];
    }
    return NULL;
}

static TermiTexture *
texture_alloc(void)
{
    for(int i = 0; i < TERMI_TEXTURE_CAP; i++) {
        if(g_textures[i].id == 0)
            return &g_textures[i];
    }
    return NULL;
}

static unsigned char *
read_file_bytes(const char *path, int *len_out)
{
    FILE *f;
    long len;
    unsigned char *data;

    if(path == NULL || len_out == NULL)
        return NULL;
    f = fopen(path, "rb");
    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if(len <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    data = (unsigned char *)malloc((size_t)len);
    if(data == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len_out = (int)len;
    return data;
}

static Color
texture_sample(const TermiTexture *tex, Rectangle source, Rectangle dest,
               int px, int py, Color tint)
{
    float u;
    float v;
    int sx;
    int sy;
    const unsigned char *p;
    Color c = {0, 0, 0, 0};

    if(tex == NULL || tex->rgba == NULL || tex->width <= 0 ||
       tex->height <= 0 || dest.width == 0.0f || dest.height == 0.0f)
        return c;
    if(source.width == 0.0f)
        source.width = (float)tex->width;
    if(source.height == 0.0f)
        source.height = (float)tex->height;
    u = ((float)px - dest.x) / dest.width;
    v = ((float)py - dest.y) / dest.height;
    if(u < 0.0f)
        u = 0.0f;
    if(v < 0.0f)
        v = 0.0f;
    if(u > 1.0f)
        u = 1.0f;
    if(v > 1.0f)
        v = 1.0f;
    sx = (int)(source.x + u * source.width);
    sy = (int)(source.y + v * source.height);
    if(sx < 0)
        sx = 0;
    if(sy < 0)
        sy = 0;
    if(sx >= tex->width)
        sx = tex->width - 1;
    if(sy >= tex->height)
        sy = tex->height - 1;
    p = &tex->rgba[((size_t)sy * (size_t)tex->width + (size_t)sx) * 4u];
    c.r = (unsigned char)(((unsigned)p[0] * (unsigned)tint.r) / 255u);
    c.g = (unsigned char)(((unsigned)p[1] * (unsigned)tint.g) / 255u);
    c.b = (unsigned char)(((unsigned)p[2] * (unsigned)tint.b) / 255u);
    c.a = (unsigned char)(((unsigned)p[3] * (unsigned)tint.a) / 255u);
    return c;
}

static void
draw_texture_cells(const TermiTexture *tex, Rectangle source, Rectangle dest,
                   Color tint)
{
    int c0 = pixel_to_col((int)dest.x);
    int r0 = pixel_to_row((int)dest.y);
    int cols = pixel_span_to_cols((int)dest.x, (int)dest.width);
    int rows = pixel_span_to_rows((int)dest.y, (int)dest.height);

    for(int y = 0; y < rows; y++) {
        for(int x = 0; x < cols; x++) {
            int px = (c0 + x) * TERMI_CELL_WIDTH + TERMI_CELL_WIDTH / 2;
            int py = (r0 + y) * TERMI_CELL_HEIGHT + TERMI_CELL_HEIGHT / 2;
            Color c = texture_sample(tex, source, dest, px, py, tint);

            if(c.a < 24)
                continue;
            cell_set(c0 + x, r0 + y, " ", pack_color(WHITE), pack_color(c),
                     TERMI_ATTR_NONE);
        }
    }
}

static int
palette_nearest(TermiPaletteColor *palette, int count, Color c)
{
    int best = 0;
    int best_dist = 0x7fffffff;

    for(int i = 0; i < count; i++) {
        int dr = (int)palette[i].r - (int)c.r;
        int dg = (int)palette[i].g - (int)c.g;
        int db = (int)palette[i].b - (int)c.b;
        int dist = dr * dr + dg * dg + db * db;

        if(dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return best;
}

static int
palette_index(TermiPaletteColor *palette, int *count, Color c)
{
    for(int i = 0; i < *count; i++) {
        if(palette[i].r == c.r && palette[i].g == c.g && palette[i].b == c.b)
            return i;
    }
    if(*count < TERMI_SIXEL_PALETTE_CAP) {
        int index = *count;

        palette[index].r = c.r;
        palette[index].g = c.g;
        palette[index].b = c.b;
        *count = index + 1;
        return index;
    }
    return palette_nearest(palette, *count, c);
}

static void
sixel_emit_texture(const TermiTexture *tex, Rectangle source, Rectangle dest,
                   Color tint)
{
    TermiPaletteColor palette[TERMI_SIXEL_PALETTE_CAP];
    int palette_count = 0;
    int width = (int)(dest.width < 0.0f ? -dest.width : dest.width);
    int height = (int)(dest.height < 0.0f ? -dest.height : dest.height);
    char seq[128];
    int col;
    int row;

    if(tex == NULL || tex->rgba == NULL || width <= 0 || height <= 0)
        return;
    if(!env_enabled("TERMI_SIXEL", 1))
        return;
    if(width > env_int("TERMI_SIXEL_MAX_WIDTH", 960))
        width = env_int("TERMI_SIXEL_MAX_WIDTH", 960);
    if(height > env_int("TERMI_SIXEL_MAX_HEIGHT", 480))
        height = env_int("TERMI_SIXEL_MAX_HEIGHT", 480);
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            Color c = texture_sample(tex, source, dest, (int)dest.x + x,
                                     (int)dest.y + y, tint);

            if(c.a >= 32)
                (void)palette_index(palette, &palette_count, c);
        }
    }
    if(palette_count == 0)
        return;
    col = pixel_to_col((int)dest.x) + 1;
    row = pixel_to_row((int)dest.y) + 1;
    if(col < 1)
        col = 1;
    if(row < 1)
        row = 1;
    snprintf(seq, sizeof(seq), "\033[%d;%dH\033Pq\"1;1;%d;%d", row, col,
             width, height);
    term_write(seq);
    for(int i = 0; i < palette_count; i++) {
        snprintf(seq, sizeof(seq), "#%d;2;%d;%d;%d", i,
                 (int)palette[i].r * 100 / 255,
                 (int)palette[i].g * 100 / 255,
                 (int)palette[i].b * 100 / 255);
        term_write(seq);
    }
    for(int band = 0; band < height; band += 6) {
        for(int pi = 0; pi < palette_count; pi++) {
            snprintf(seq, sizeof(seq), "#%d", pi);
            term_write(seq);
            for(int x = 0; x < width; x++) {
                int bits = 0;

                for(int bit = 0; bit < 6; bit++) {
                    int y = band + bit;
                    Color c;
                    int ci;

                    if(y >= height)
                        continue;
                    c = texture_sample(tex, source, dest, (int)dest.x + x,
                                       (int)dest.y + y, tint);
                    if(c.a < 32)
                        continue;
                    ci = palette_nearest(palette, palette_count, c);
                    if(ci == pi)
                        bits |= 1 << bit;
                }
                seq[0] = (char)(63 + bits);
                term_write_len(seq, 1);
            }
            term_write("$");
        }
        term_write("-");
    }
    term_write("\033\\");
}

static int
sixel_op_equal(const TermiSixelOp *a, const TermiSixelOp *b)
{
    return a->texture_id == b->texture_id &&
           a->source.x == b->source.x && a->source.y == b->source.y &&
           a->source.width == b->source.width &&
           a->source.height == b->source.height &&
           a->dest.x == b->dest.x && a->dest.y == b->dest.y &&
           a->dest.width == b->dest.width &&
           a->dest.height == b->dest.height &&
           a->tint.r == b->tint.r && a->tint.g == b->tint.g &&
           a->tint.b == b->tint.b && a->tint.a == b->tint.a;
}

static int
sixel_op_seen_last_frame(const TermiSixelOp *op)
{
    for(int i = 0; i < g_prev_sixel_count; i++) {
        if(sixel_op_equal(op, &g_prev_sixel_queue[i]))
            return 1;
    }
    return 0;
}

static int
sixel_op_overlaps_dirty_cells(const TermiSixelOp *op)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if(dirty_empty())
        return 0;
    x0 = pixel_to_col((int)op->dest.x);
    y0 = pixel_to_row((int)op->dest.y);
    x1 = pixel_to_col((int)(op->dest.x + op->dest.width - 1.0f));
    y1 = pixel_to_row((int)(op->dest.y + op->dest.height - 1.0f));
    if(x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if(y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    return x0 <= g_dirty_max_x && x1 >= g_dirty_min_x &&
           y0 <= g_dirty_max_y && y1 >= g_dirty_min_y;
}

static int
sixel_op_can_skip(const TermiSixelOp *op)
{
    if(!env_enabled("TERMI_SIXEL_CACHE", 1))
        return 0;
    return sixel_op_seen_last_frame(op) && !sixel_op_overlaps_dirty_cells(op);
}

static void
queue_sixel(unsigned texture_id, Rectangle source, Rectangle dest, Color tint)
{
    if(g_sixel_count >= TERMI_SIXEL_QUEUE_CAP)
        return;
    if(dest.x >= (float)GetScreenWidth() || dest.y >= (float)GetScreenHeight() ||
       dest.x + dest.width <= 0.0f || dest.y + dest.height <= 0.0f)
        return;
    g_sixel_queue[g_sixel_count++] =
        (TermiSixelOp){texture_id, source, dest, tint};
}

static void
present_sixel_queue(void)
{
    for(int i = 0; i < g_sixel_count; i++) {
        TermiSixelOp *op = &g_sixel_queue[i];

        if(!sixel_op_can_skip(op))
            sixel_emit_texture(texture_find(op->texture_id), op->source,
                               op->dest, op->tint);
    }
    g_prev_sixel_count = g_sixel_count;
    if(g_prev_sixel_count > 0)
        memcpy(g_prev_sixel_queue, g_sixel_queue,
               (size_t)g_prev_sixel_count * sizeof(g_prev_sixel_queue[0]));
    if(g_sixel_count > 0)
        fflush(stdout);
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    Image img = {0};
    int width = 0;
    int height = 0;

    (void)fileType;
    img.data = kry_decode_image_rgba(fileData, dataSize, &width, &height);
    if(img.data != NULL) {
        img.width = width;
        img.height = height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    }
    return img;
}

Image LoadImage(const char *fileName)
{
    unsigned char *data;
    int len = 0;
    Image img = {0};

    data = read_file_bytes(fileName, &len);
    if(data == NULL)
        return img;
    img = LoadImageFromMemory("", data, len);
    free(data);
    return img;
}

bool IsImageValid(Image image)
{
    return image.data != NULL && image.width > 0 && image.height > 0;
}

void UnloadImage(Image image) { free(image.data); }

void ImageFormat(Image *image, int newFormat)
{
    if(image != NULL)
        image->format = newFormat;
}

Image GenImageColor(int width, int height, Color color)
{
    Image img = {0};
    unsigned char *pixels;

    if(width <= 0 || height <= 0)
        return img;
    pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4u);
    if(pixels == NULL)
        return img;
    for(int i = 0; i < width * height; i++) {
        pixels[i * 4 + 0] = color.r;
        pixels[i * 4 + 1] = color.g;
        pixels[i * 4 + 2] = color.b;
        pixels[i * 4 + 3] = color.a;
    }
    img.data = pixels;
    img.width = width;
    img.height = height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}

Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex = {0};
    TermiTexture *slot;
    size_t bytes;

    if(image.width <= 0 || image.height <= 0)
        return tex;
    slot = texture_alloc();
    if(slot == NULL)
        return tex;
    memset(slot, 0, sizeof(*slot));
    bytes = (size_t)image.width * (size_t)image.height * 4u;
    if(image.data != NULL && bytes > 0) {
        slot->rgba = (unsigned char *)malloc(bytes);
        if(slot->rgba == NULL)
            return tex;
        memcpy(slot->rgba, image.data, bytes);
    }
    slot->id = g_next_texture_id++;
    slot->width = image.width;
    slot->height = image.height;
    slot->format = image.format;

    tex.id = slot->id;
    tex.width = image.width > 0 ? image.width : TERMI_CELL_WIDTH;
    tex.height = image.height > 0 ? image.height : TERMI_CELL_HEIGHT;
    tex.mipmaps = 1;
    tex.format = image.format;
    return tex;
}
Texture2D LoadTexture(const char *fileName)
{
    Image image;
    Texture2D tex;

    image = LoadImage(fileName);
    tex = LoadTextureFromImage(image);
    UnloadImage(image);
    return tex;
}
bool IsTextureValid(Texture2D texture) { return texture.id != 0; }
void UnloadTexture(Texture2D texture)
{
    TermiTexture *slot = texture_find(texture.id);

    if(slot == NULL)
        return;
    free(slot->rgba);
    memset(slot, 0, sizeof(*slot));
}
void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width,
                                        (float)texture.height},
                   (Rectangle){(float)posX, (float)posY, (float)texture.width,
                               (float)texture.height},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTextureV(Texture2D texture, Vector2 position, Color tint)
{
    DrawTexture(texture, (int)position.x, (int)position.y, tint);
}
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation,
                   float scale, Color tint)
{
    DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width,
                                        (float)texture.height},
                   (Rectangle){position.x, position.y,
                               (float)texture.width * scale,
                               (float)texture.height * scale},
                   (Vector2){0, 0}, rotation, tint);
}
void DrawTextureRec(Texture2D texture, Rectangle rec, Vector2 position,
                    Color tint)
{
    DrawTexturePro(texture, rec,
                   (Rectangle){position.x, position.y, rec.width, rec.height},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    TermiTexture *tex = texture_find(texture.id);

    (void)origin;
    (void)rotation;
    if(tex != NULL && tex->rgba != NULL) {
        draw_texture_cells(tex, source, dest, tint);
        queue_sixel(texture.id, source, dest, tint);
        return;
    }
    DrawRectangleRec(dest, tint);
}

void SetTraceLogLevel(int logLevel) { g_trace_level = logLevel; }
void SetTraceLogCallback(TraceLogCallback callback) { g_trace_callback = callback; }
void
TraceLog(int logLevel, const char *text, ...)
{
    va_list args;

    if(logLevel < g_trace_level)
        return;
    va_start(args, text);
    if(g_trace_callback != NULL) {
        g_trace_callback(logLevel, text, args);
    } else if(getenv("TERMI_LOG") != NULL) {
        vfprintf(stderr, text != NULL ? text : "", args);
        fputc('\n', stderr);
    }
    va_end(args);
}

const char *
TextFormat(const char *text, ...)
{
    static char buffers[4][1024];
    static int index;
    va_list args;
    char *out = buffers[index++ % 4];

    va_start(args, text);
    vsnprintf(out, 1024, text != NULL ? text : "", args);
    va_end(args);
    return out;
}
