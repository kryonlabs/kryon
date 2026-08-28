/*
 * kry_inject.c - synthetic input event queue.
 *
 * Pure state: no window, no GPU, no platform. Down-state changes made by
 * inject calls between pumps show up as pressed/released edges for exactly
 * one frame, the same way raylib derives edges by comparing the previous
 * and current poll state. A tap schedules its own release on the following
 * pump, so a click reads press -> down -> release across two frames - what
 * UIHandleClick-based widgets expect. The generated input wrappers consult
 * the query side before real platform input.
 */
#include "kryon.h"
#include "kry_inject.h"

#include <string.h>

typedef struct {
    int button;
    int down;
    int delay;   /* pumps to wait before firing */
} KryInjectMouseEvent;

typedef struct {
    int key;
    int down;
    int delay;
} InjectKeyEvent;

static int g_inject_have_mouse;
static int g_inject_pos_frames;   /* pumps the position was requested for */
static int g_inject_pos_serving;  /* position applies during this frame */
static float g_inject_mouse_x;
static float g_inject_mouse_y;
static float g_inject_prev_x;
static float g_inject_prev_y;
static float g_inject_delta_x;
static float g_inject_delta_y;
static float g_inject_wheel;
static float g_inject_wheel_frame;

static unsigned char g_inject_button_down[KRY_INJECT_MAX_BUTTONS];
static unsigned char g_inject_button_prev[KRY_INJECT_MAX_BUTTONS];
static unsigned char g_inject_button_pressed[KRY_INJECT_MAX_BUTTONS];
static unsigned char g_inject_button_released[KRY_INJECT_MAX_BUTTONS];

static unsigned char g_inject_key_down[KRY_INJECT_KEY_MAX];
static unsigned char g_inject_key_prev[KRY_INJECT_KEY_MAX];
static unsigned char g_inject_key_pressed[KRY_INJECT_KEY_MAX];
static unsigned char g_inject_key_released[KRY_INJECT_KEY_MAX];

static int g_inject_layout_codes[KRY_INJECT_CHAR_QUEUE];
static unsigned char g_inject_layout_down[KRY_INJECT_CHAR_QUEUE];
static unsigned char g_inject_layout_prev[KRY_INJECT_CHAR_QUEUE];
static unsigned char g_inject_layout_pressed[KRY_INJECT_CHAR_QUEUE];
static unsigned char g_inject_layout_released[KRY_INJECT_CHAR_QUEUE];
static int g_inject_layout_count;

static int g_inject_chars[KRY_INJECT_CHAR_QUEUE];
static int g_inject_char_count;
static int g_inject_key_queue[KRY_INJECT_CHAR_QUEUE];
static int g_inject_key_queue_count;

static KryInjectMouseEvent g_inject_mouse_events[16];
static int g_inject_mouse_event_count;
static InjectKeyEvent g_inject_key_events[16];
static int g_inject_key_event_count;
static InjectKeyEvent g_inject_layout_events[16];
static int g_inject_layout_event_count;

static int
inject_layout_codepoint(int codepoint)
{
    if(codepoint >= 'A' && codepoint <= 'Z')
        return codepoint - 'A' + 'a';
    return codepoint;
}

static void
kry_inject_queue_mouse_event(int button, int down, int delay)
{
    if(g_inject_mouse_event_count >=
       (int)(sizeof(g_inject_mouse_events) / sizeof(g_inject_mouse_events[0])))
        return;
    g_inject_mouse_events[g_inject_mouse_event_count].button = button;
    g_inject_mouse_events[g_inject_mouse_event_count].down = down;
    g_inject_mouse_events[g_inject_mouse_event_count].delay = delay;
    g_inject_mouse_event_count++;
}

static void
kry_inject_queue_key_event(int key, int down, int delay)
{
    if(g_inject_key_event_count >=
       (int)(sizeof(g_inject_key_events) / sizeof(g_inject_key_events[0])))
        return;
    g_inject_key_events[g_inject_key_event_count].key = key;
    g_inject_key_events[g_inject_key_event_count].down = down;
    g_inject_key_events[g_inject_key_event_count].delay = delay;
    g_inject_key_event_count++;
}

static void
inject_queue_layout_event(int codepoint, int down, int delay)
{
    if(g_inject_layout_event_count >=
       (int)(sizeof(g_inject_layout_events) /
             sizeof(g_inject_layout_events[0])))
        return;
    g_inject_layout_events[g_inject_layout_event_count].key = codepoint;
    g_inject_layout_events[g_inject_layout_event_count].down = down;
    g_inject_layout_events[g_inject_layout_event_count].delay = delay;
    g_inject_layout_event_count++;
}

static int
inject_layout_index(int codepoint)
{
    int normalized = inject_layout_codepoint(codepoint);

    for(int i = 0; i < g_inject_layout_count; i++) {
        if(g_inject_layout_codes[i] == normalized)
            return i;
    }
    return -1;
}

static int
inject_layout_slot(int codepoint)
{
    int index;

    codepoint = inject_layout_codepoint(codepoint);
    index = inject_layout_index(codepoint);
    if(index >= 0)
        return index;
    if(codepoint <= 0 || g_inject_layout_count >= KRY_INJECT_CHAR_QUEUE)
        return -1;
    index = g_inject_layout_count++;
    g_inject_layout_codes[index] = codepoint;
    g_inject_layout_down[index] = 0;
    g_inject_layout_prev[index] = 0;
    g_inject_layout_pressed[index] = 0;
    g_inject_layout_released[index] = 0;
    return index;
}

void
InjectMousePosition(float x, float y)
{
    g_inject_have_mouse = 1;
    if(g_inject_pos_frames < 1)
        g_inject_pos_frames = 1;
    g_inject_mouse_x = x;
    g_inject_mouse_y = y;
}

void
InjectMouseButton(int button, int down)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return;
    g_inject_have_mouse = 1;
    g_inject_button_down[button] = down != 0;
}

void
InjectKey(int key, int down)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return;
    if(down && !g_inject_key_down[key] &&
       g_inject_key_queue_count < KRY_INJECT_CHAR_QUEUE)
        g_inject_key_queue[g_inject_key_queue_count++] = key;
    g_inject_key_down[key] = down != 0;
}

void
InjectKeyTap(int key)
{
    InjectKey(key, 1);
    kry_inject_queue_key_event(key, 0, 1);   /* release next pump */
}

void
InjectLayoutKey(int codepoint, int down)
{
    int index = inject_layout_slot(codepoint);

    if(index < 0)
        return;
    g_inject_layout_down[index] = down != 0;
}

void
InjectLayoutKeyTap(int codepoint)
{
    InjectLayoutKey(codepoint, 1);
    inject_queue_layout_event(codepoint, 0, 1);
}

void
InjectText(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;

    if(text == NULL)
        return;
    while(*p != '\0' && g_inject_char_count < KRY_INJECT_CHAR_QUEUE) {
        unsigned codepoint;
        int bytes;

        if(*p < 0x80) {
            codepoint = *p;
            bytes = 1;
        } else if((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
            codepoint = ((unsigned)(p[0] & 0x1f) << 6) |
                        (unsigned)(p[1] & 0x3f);
            bytes = 2;
        } else if((*p & 0xf0) == 0xe0 && p[1] != '\0' &&
                  (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
            codepoint = ((unsigned)(p[0] & 0x0f) << 12) |
                        ((unsigned)(p[1] & 0x3f) << 6) |
                        (unsigned)(p[2] & 0x3f);
            bytes = 3;
        } else if((*p & 0xf8) == 0xf0 && p[1] != '\0' && p[2] != '\0' &&
                  (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
                  (p[3] & 0xc0) == 0x80) {
            codepoint = ((unsigned)(p[0] & 0x07) << 18) |
                        ((unsigned)(p[1] & 0x3f) << 12) |
                        ((unsigned)(p[2] & 0x3f) << 6) |
                        (unsigned)(p[3] & 0x3f);
            bytes = 4;
        } else {
            codepoint = 0xfffd;
            bytes = 1;
        }
        g_inject_chars[g_inject_char_count++] = (int)codepoint;
        p += bytes;
    }
}

void
InjectWheel(float move)
{
    g_inject_have_mouse = 1;
    g_inject_wheel += move;
}

void
InjectTap(float x, float y)
{
    InjectMousePosition(x, y);
    g_inject_pos_frames = 2;   /* press frame + release frame */
    InjectMouseButton(0, 1);
    kry_inject_queue_mouse_event(0, 0, 1);   /* release next pump */
}

void
InjectPump(void)
{
    int i;

    /* fire due scheduled events first so their edges count this frame */
    for(i = 0; i < g_inject_mouse_event_count; ) {
        if(g_inject_mouse_events[i].delay > 0) {
            g_inject_mouse_events[i].delay--;
            i++;
            continue;
        }
        InjectMouseButton(g_inject_mouse_events[i].button,
                               g_inject_mouse_events[i].down);
        g_inject_mouse_event_count--;
        memmove(&g_inject_mouse_events[i],
                &g_inject_mouse_events[i + 1],
                (size_t)(g_inject_mouse_event_count - i) *
                    sizeof(g_inject_mouse_events[0]));
    }
    for(i = 0; i < g_inject_key_event_count; ) {
        if(g_inject_key_events[i].delay > 0) {
            g_inject_key_events[i].delay--;
            i++;
            continue;
        }
        InjectKey(g_inject_key_events[i].key,
                       g_inject_key_events[i].down);
        g_inject_key_event_count--;
        memmove(&g_inject_key_events[i],
                &g_inject_key_events[i + 1],
                (size_t)(g_inject_key_event_count - i) *
                    sizeof(g_inject_key_events[0]));
    }
    for(i = 0; i < g_inject_layout_event_count; ) {
        if(g_inject_layout_events[i].delay > 0) {
            g_inject_layout_events[i].delay--;
            i++;
            continue;
        }
        InjectLayoutKey(g_inject_layout_events[i].key,
                        g_inject_layout_events[i].down);
        g_inject_layout_event_count--;
        memmove(&g_inject_layout_events[i],
                &g_inject_layout_events[i + 1],
                (size_t)(g_inject_layout_event_count - i) *
                    sizeof(g_inject_layout_events[0]));
    }
    /* derive this frame's edges from down-state changes */
    for(i = 0; i < KRY_INJECT_MAX_BUTTONS; i++) {
        g_inject_button_pressed[i] =
            g_inject_button_down[i] && !g_inject_button_prev[i];
        g_inject_button_released[i] =
            !g_inject_button_down[i] && g_inject_button_prev[i];
        g_inject_button_prev[i] = g_inject_button_down[i];
    }
    for(i = 0; i < KRY_INJECT_KEY_MAX; i++) {
        g_inject_key_pressed[i] = g_inject_key_down[i] && !g_inject_key_prev[i];
        g_inject_key_released[i] =
            !g_inject_key_down[i] && g_inject_key_prev[i];
        g_inject_key_prev[i] = g_inject_key_down[i];
    }
    for(i = 0; i < g_inject_layout_count; i++) {
        g_inject_layout_pressed[i] =
            g_inject_layout_down[i] && !g_inject_layout_prev[i];
        g_inject_layout_released[i] =
            !g_inject_layout_down[i] && g_inject_layout_prev[i];
        g_inject_layout_prev[i] = g_inject_layout_down[i];
    }
    g_inject_delta_x = g_inject_mouse_x - g_inject_prev_x;
    g_inject_delta_y = g_inject_mouse_y - g_inject_prev_y;
    /* the position serves this frame, then lapses unless renewed */
    if(g_inject_pos_frames > 0) {
        g_inject_pos_serving = 1;
        g_inject_pos_frames--;
        g_inject_prev_x = g_inject_mouse_x;
        g_inject_prev_y = g_inject_mouse_y;
    } else {
        g_inject_pos_serving = 0;
    }
    g_inject_wheel_frame = g_inject_wheel;
    g_inject_wheel = 0;
}

int
InjectMouseActive(void)
{
    return g_inject_have_mouse && g_inject_pos_serving;
}

float
InjectMouseX(void)
{
    return g_inject_mouse_x;
}

float
InjectMouseY(void)
{
    return g_inject_mouse_y;
}

float
InjectMouseDeltaX(void)
{
    return g_inject_delta_x;
}

float
InjectMouseDeltaY(void)
{
    return g_inject_delta_y;
}

float
InjectWheelValue(void)
{
    return g_inject_wheel_frame;
}

int
InjectMousePressed(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_pressed[button];
}

int
InjectMouseReleased(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_released[button];
}

int
InjectMouseButtonDown(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_down[button];
}

int
InjectMouseButtonUp(int button)
{
    return !InjectMouseButtonDown(button);
}

int
InjectKeyPressed(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_pressed[key];
}

int
InjectKeyReleased(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_released[key];
}

int
InjectKeyDown(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_down[key];
}

int
InjectLayoutKeyPressed(int codepoint)
{
    int index = inject_layout_index(codepoint);

    if(index < 0)
        return 0;
    return g_inject_layout_pressed[index];
}

int
InjectLayoutKeyReleased(int codepoint)
{
    int index = inject_layout_index(codepoint);

    if(index < 0)
        return 0;
    return g_inject_layout_released[index];
}

int
InjectLayoutKeyDown(int codepoint)
{
    int index = inject_layout_index(codepoint);

    if(index < 0)
        return 0;
    return g_inject_layout_down[index];
}

int
InjectCharPressed(void)
{
    int i;

    if(g_inject_char_count == 0)
        return 0;
    {
        int code = g_inject_chars[0];

        g_inject_char_count--;
        for(i = 0; i < g_inject_char_count; i++)
            g_inject_chars[i] = g_inject_chars[i + 1];
        return code;
    }
}

int
InjectKeyPressedCode(void)
{
    int i;

    if(g_inject_key_queue_count == 0)
        return 0;
    {
        int key = g_inject_key_queue[0];

        g_inject_key_queue_count--;
        for(i = 0; i < g_inject_key_queue_count; i++)
            g_inject_key_queue[i] = g_inject_key_queue[i + 1];
        return key;
    }
}

void
InjectReset(void)
{
    g_inject_have_mouse = 0;
    g_inject_pos_frames = 0;
    g_inject_pos_serving = 0;
    g_inject_mouse_x = 0;
    g_inject_mouse_y = 0;
    g_inject_prev_x = 0;
    g_inject_prev_y = 0;
    g_inject_delta_x = 0;
    g_inject_delta_y = 0;
    g_inject_wheel = 0;
    g_inject_wheel_frame = 0;
    memset(g_inject_button_down, 0, sizeof(g_inject_button_down));
    memset(g_inject_button_prev, 0, sizeof(g_inject_button_prev));
    memset(g_inject_button_pressed, 0, sizeof(g_inject_button_pressed));
    memset(g_inject_button_released, 0, sizeof(g_inject_button_released));
    memset(g_inject_key_down, 0, sizeof(g_inject_key_down));
    memset(g_inject_key_prev, 0, sizeof(g_inject_key_prev));
    memset(g_inject_key_pressed, 0, sizeof(g_inject_key_pressed));
    memset(g_inject_key_released, 0, sizeof(g_inject_key_released));
    memset(g_inject_layout_codes, 0, sizeof(g_inject_layout_codes));
    memset(g_inject_layout_down, 0, sizeof(g_inject_layout_down));
    memset(g_inject_layout_prev, 0, sizeof(g_inject_layout_prev));
    memset(g_inject_layout_pressed, 0, sizeof(g_inject_layout_pressed));
    memset(g_inject_layout_released, 0, sizeof(g_inject_layout_released));
    g_inject_layout_count = 0;
    g_inject_char_count = 0;
    g_inject_key_queue_count = 0;
    g_inject_mouse_event_count = 0;
    g_inject_key_event_count = 0;
    g_inject_layout_event_count = 0;
}
