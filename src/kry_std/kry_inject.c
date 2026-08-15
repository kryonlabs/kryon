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
} KryInjectKeyEvent;

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

static int g_inject_chars[KRY_INJECT_CHAR_QUEUE];
static int g_inject_char_count;
static int g_inject_key_queue[KRY_INJECT_CHAR_QUEUE];
static int g_inject_key_queue_count;

static KryInjectMouseEvent g_inject_mouse_events[16];
static int g_inject_mouse_event_count;
static KryInjectKeyEvent g_inject_key_events[16];
static int g_inject_key_event_count;

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

void
KryonInjectMousePosition(float x, float y)
{
    g_inject_have_mouse = 1;
    if(g_inject_pos_frames < 1)
        g_inject_pos_frames = 1;
    g_inject_mouse_x = x;
    g_inject_mouse_y = y;
}

void
KryonInjectMouseButton(int button, int down)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return;
    g_inject_have_mouse = 1;
    g_inject_button_down[button] = down != 0;
}

void
KryonInjectKey(int key, int down)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return;
    if(down && !g_inject_key_down[key] &&
       g_inject_key_queue_count < KRY_INJECT_CHAR_QUEUE)
        g_inject_key_queue[g_inject_key_queue_count++] = key;
    g_inject_key_down[key] = down != 0;
}

void
KryonInjectKeyTap(int key)
{
    KryonInjectKey(key, 1);
    kry_inject_queue_key_event(key, 0, 1);   /* release next pump */
}

void
KryonInjectText(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;

    if(text == NULL)
        return;
    while(*p != '\0' && g_inject_char_count < KRY_INJECT_CHAR_QUEUE)
        g_inject_chars[g_inject_char_count++] = *p++;
}

void
KryonInjectWheel(float move)
{
    g_inject_have_mouse = 1;
    g_inject_wheel += move;
}

void
KryonInjectTap(float x, float y)
{
    KryonInjectMousePosition(x, y);
    g_inject_pos_frames = 2;   /* press frame + release frame */
    KryonInjectMouseButton(0, 1);
    kry_inject_queue_mouse_event(0, 0, 1);   /* release next pump */
}

void
KryonInjectPump(void)
{
    int i;

    /* fire due scheduled events first so their edges count this frame */
    for(i = 0; i < g_inject_mouse_event_count; ) {
        if(g_inject_mouse_events[i].delay > 0) {
            g_inject_mouse_events[i].delay--;
            i++;
            continue;
        }
        KryonInjectMouseButton(g_inject_mouse_events[i].button,
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
        KryonInjectKey(g_inject_key_events[i].key,
                       g_inject_key_events[i].down);
        g_inject_key_event_count--;
        memmove(&g_inject_key_events[i],
                &g_inject_key_events[i + 1],
                (size_t)(g_inject_key_event_count - i) *
                    sizeof(g_inject_key_events[0]));
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
KryonInjectMouseActive(void)
{
    return g_inject_have_mouse && g_inject_pos_serving;
}

float
KryonInjectMouseX(void)
{
    return g_inject_mouse_x;
}

float
KryonInjectMouseY(void)
{
    return g_inject_mouse_y;
}

float
KryonInjectMouseDeltaX(void)
{
    return g_inject_delta_x;
}

float
KryonInjectMouseDeltaY(void)
{
    return g_inject_delta_y;
}

float
KryonInjectWheelValue(void)
{
    return g_inject_wheel_frame;
}

int
KryonInjectMousePressed(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_pressed[button];
}

int
KryonInjectMouseReleased(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_released[button];
}

int
KryonInjectMouseButtonDown(int button)
{
    if(button < 0 || button >= KRY_INJECT_MAX_BUTTONS)
        return 0;
    return g_inject_button_down[button];
}

int
KryonInjectMouseButtonUp(int button)
{
    return !KryonInjectMouseButtonDown(button);
}

int
KryonInjectKeyPressed(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_pressed[key];
}

int
KryonInjectKeyReleased(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_released[key];
}

int
KryonInjectKeyDown(int key)
{
    if(key <= 0 || key >= KRY_INJECT_KEY_MAX)
        return 0;
    return g_inject_key_down[key];
}

int
KryonInjectCharPressed(void)
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
KryonInjectKeyPressedCode(void)
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
KryonInjectReset(void)
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
    g_inject_char_count = 0;
    g_inject_key_queue_count = 0;
    g_inject_mouse_event_count = 0;
    g_inject_key_event_count = 0;
}
