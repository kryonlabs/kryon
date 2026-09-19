/* Shared input front-end; see kry_input.h. The public input queries merge
 * synthetic injection, the modal input override, and the keyboard platform
 * callbacks around the active backends BackendRaw_* hooks, so every
 * backend gets identical input behavior. Previously this logic was emitted
 * only into the generated raylib wrappers (tools/generate-kryon-compat.sh). */

#include "kry_input.h"
#include "kry_input_internal.h"
#include "kry_inject.h"
#include "app_host.h"

#include <stddef.h>
#include <string.h>

#if ANDROID_BUILD
#include <android/input.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);
static int32_t (*android_previous_input)(struct android_app *, AInputEvent *);
static int android_frame_press;
static int android_frame_release;
static int android_frame_cancel;
static int android_pointer_down;
static Vector2 android_pointer_position;
static int android_drag_sequence_active;
static Vector2 android_drag_sequence_start;
static int android_frame_drag_valid;
static Vector2 android_frame_drag_start;
static Vector2 android_frame_drag_current;

static int32_t
android_track_input(struct android_app *app, AInputEvent *event)
{
    if(AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        int pointer_count = AMotionEvent_getPointerCount(event);
        if(pointer_count > 0) {
            android_pointer_position = (Vector2){
                AMotionEvent_getX(event, 0),
                AMotionEvent_getY(event, 0)
            };
        }
        if(action == AMOTION_EVENT_ACTION_DOWN ||
           action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            android_frame_press = 1;
            android_pointer_down = 1;
            android_drag_sequence_active = 1;
            android_drag_sequence_start = android_pointer_position;
            android_frame_drag_start = android_pointer_position;
            android_frame_drag_current = android_pointer_position;
        }
        if(action == AMOTION_EVENT_ACTION_MOVE && pointer_count > 0) {
            android_pointer_down = 1;
            if(android_drag_sequence_active) {
                android_frame_drag_valid = 1;
                android_frame_drag_start = android_drag_sequence_start;
                android_frame_drag_current = android_pointer_position;
            }
        }
        if(action == AMOTION_EVENT_ACTION_UP ||
           action == AMOTION_EVENT_ACTION_POINTER_UP) {
            android_frame_release = 1;
            android_pointer_down = 0;
            if(android_drag_sequence_active) {
                android_frame_drag_valid = 1;
                android_frame_drag_start = android_drag_sequence_start;
                android_frame_drag_current = android_pointer_position;
            }
            android_drag_sequence_active = 0;
        }
        if(action == AMOTION_EVENT_ACTION_CANCEL) {
            android_frame_press = 0;
            android_frame_release = 0;
            android_frame_cancel = 1;
            android_pointer_down = 0;
            android_drag_sequence_active = 0;
        }
    }
    return android_previous_input != NULL
        ? android_previous_input(app, event) : 0;
}

int
kry_android_touch_down(void)
{
    return android_pointer_down;
}

Vector2
kry_android_touch_position(void)
{
    return android_pointer_position;
}

int
kry_android_frame_drag(Vector2 *start, Vector2 *current)
{
    if(!android_frame_drag_valid)
        return 0;
    if(start != NULL)
        *start = android_frame_drag_start;
    if(current != NULL)
        *current = android_frame_drag_current;
    return 1;
}

void
kry_android_consume_frame_drag(void)
{
    android_frame_drag_valid = 0;
}

void
kry_android_prepare_input_poll(void)
{
    struct android_app *app = GetAndroidApp();
    if(app != NULL && app->onInputEvent != android_track_input) {
        android_previous_input = app->onInputEvent;
        app->onInputEvent = android_track_input;
    }
    /* Preserve both edges when Android drains a quick tap in one poll. */
    android_frame_press = 0;
    android_frame_release = 0;
    android_frame_cancel = 0;
    android_frame_drag_valid = 0;
}
#endif

#ifdef KRYON_BACKEND_RAYLIB
#include <SDL.h>

static Uint32 sdl_core_window_id;
static SDL_atomic_t sdl_pending_press[8];
static SDL_atomic_t sdl_pending_release[8];
static int sdl_frame_press[8];
static int sdl_frame_release[8];
static Vector2 sdl_pending_press_position;
static Vector2 sdl_frame_press_position;

static int
sdl_pointer_event(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if(event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEBUTTONUP)
        return 1;
    if(sdl_core_window_id == 0 || event->button.windowID != sdl_core_window_id)
        return 1;
    int button = event->button.button - 1;
    if(event->button.button == SDL_BUTTON_RIGHT)
        button = MOUSE_BUTTON_RIGHT;
    else if(event->button.button == SDL_BUTTON_MIDDLE)
        button = MOUSE_BUTTON_MIDDLE;
    if(button == MOUSE_BUTTON_LEFT && event->type == SDL_MOUSEBUTTONDOWN)
        sdl_pending_press_position = (Vector2){event->button.x, event->button.y};
    if(button >= 0 && button < 8) {
        SDL_AtomicSet(event->type == SDL_MOUSEBUTTONDOWN
            ? &sdl_pending_press[button] : &sdl_pending_release[button], 1);
    }
    return 1;
}

void
kry_sdl_prepare_input_poll(void)
{
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    sdl_core_window_id = window != NULL ? SDL_GetWindowID(window) : 0;
    /* Reinstall after a possible window/SDL restart without duplicating watches. */
    SDL_DelEventWatch(sdl_pointer_event, NULL);
    SDL_AddEventWatch(sdl_pointer_event, NULL);
}

void
kry_sdl_finish_input_poll(void)
{
    /* Preserve a complete tap even when SDL drains both edges in one poll. */
    sdl_frame_press_position = sdl_pending_press_position;
    for(int button = 0; button < 8; button++) {
        sdl_frame_press[button] = SDL_AtomicSet(&sdl_pending_press[button], 0);
        sdl_frame_release[button] = SDL_AtomicSet(&sdl_pending_release[button], 0);
    }
}
#endif

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const KryonInputOverride kryon_zero_kryoninputoverride;


#define KRYON_INPUT_OVERRIDE_STACK_CAP 8
static KryonInputOverride g_kryon_input_override = {0};
static KryonInputOverride g_kryon_input_override_stack[KRYON_INPUT_OVERRIDE_STACK_CAP];
static int g_kryon_input_override_depth = 0;
static int g_kryon_keyboard_input_enabled = 1;
static KeyInputPlatformCallback g_kryon_key_input_update_callback = NULL;
static KeyPlatformCallback g_kryon_key_pressed_callback = NULL;
static KeyPlatformCallback g_kryon_key_down_callback = NULL;

#define KRY_TEXT_COMPOSITION_QUEUE_CAP 16
static KryTextCompositionEvent
    g_text_composition_queue[KRY_TEXT_COMPOSITION_QUEUE_CAP];
static int g_text_composition_head;
static int g_text_composition_count;

static void
kry_text_composition_trim_utf8(char *text)
{
    size_t len;
    size_t start;
    int expected = 1;
    unsigned char lead;

    if(text == NULL || text[0] == '\0')
        return;
    len = strlen(text);
    start = len - 1;
    while(start > 0 && (((unsigned char)text[start] & 0xc0) == 0x80))
        start--;
    lead = (unsigned char)text[start];
    if((lead & 0xe0) == 0xc0)
        expected = 2;
    else if((lead & 0xf0) == 0xe0)
        expected = 3;
    else if((lead & 0xf8) == 0xf0)
        expected = 4;
    if(start + (size_t)expected > len)
        text[start] = '\0';
}

int
SubmitTextComposition(KryTextCompositionPhase phase, const char *text,
                      int cursor, int selection_length)
{
    KryTextCompositionEvent *event;
    int tail;

    if(phase < KRY_TEXT_COMPOSITION_START ||
       phase > KRY_TEXT_COMPOSITION_CANCEL ||
       g_text_composition_count >= KRY_TEXT_COMPOSITION_QUEUE_CAP)
        return 0;
    tail = (g_text_composition_head + g_text_composition_count) %
           KRY_TEXT_COMPOSITION_QUEUE_CAP;
    event = &g_text_composition_queue[tail];
    memset(event, 0, sizeof(*event));
    event->phase = phase;
    event->cursor = cursor >= 0 ? cursor : 0;
    event->selection_length = selection_length >= 0 ? selection_length : 0;
    if(text != NULL) {
        strncpy(event->text, text, sizeof(event->text) - 1);
        event->text[sizeof(event->text) - 1] = '\0';
        kry_text_composition_trim_utf8(event->text);
    }
    g_text_composition_count++;
    return 1;
}

int
PollTextComposition(KryTextCompositionEvent *event)
{
    if(event == NULL || g_text_composition_count <= 0)
        return 0;
    *event = g_text_composition_queue[g_text_composition_head];
    g_text_composition_head = (g_text_composition_head + 1) %
                              KRY_TEXT_COMPOSITION_QUEUE_CAP;
    g_text_composition_count--;
    return 1;
}

void
ClearTextComposition(void)
{
    g_text_composition_head = 0;
    g_text_composition_count = 0;
}

static int k_key_prefers_platform(int key)
{
    return key >= 32 && key <= 126;
}

static int k_layout_codepoint(int codepoint)
{
    if(codepoint >= 'A' && codepoint <= 'Z')
        return codepoint - 'A' + 'a';
    return codepoint;
}

static int k_layout_key_matches(int key, int codepoint)
{
    return GetLayoutKeyCodepoint(key) == k_layout_codepoint(codepoint);
}

#ifdef KRYON_BACKEND_RAYLIB
static SDL_Scancode k_sdl_scancode_for_key(int key)
{
    switch(key) {
    case KEY_A: return SDL_SCANCODE_A;
    case KEY_B: return SDL_SCANCODE_B;
    case KEY_C: return SDL_SCANCODE_C;
    case KEY_D: return SDL_SCANCODE_D;
    case KEY_E: return SDL_SCANCODE_E;
    case KEY_F: return SDL_SCANCODE_F;
    case KEY_G: return SDL_SCANCODE_G;
    case KEY_H: return SDL_SCANCODE_H;
    case KEY_I: return SDL_SCANCODE_I;
    case KEY_J: return SDL_SCANCODE_J;
    case KEY_K: return SDL_SCANCODE_K;
    case KEY_L: return SDL_SCANCODE_L;
    case KEY_M: return SDL_SCANCODE_M;
    case KEY_N: return SDL_SCANCODE_N;
    case KEY_O: return SDL_SCANCODE_O;
    case KEY_P: return SDL_SCANCODE_P;
    case KEY_Q: return SDL_SCANCODE_Q;
    case KEY_R: return SDL_SCANCODE_R;
    case KEY_S: return SDL_SCANCODE_S;
    case KEY_T: return SDL_SCANCODE_T;
    case KEY_U: return SDL_SCANCODE_U;
    case KEY_V: return SDL_SCANCODE_V;
    case KEY_W: return SDL_SCANCODE_W;
    case KEY_X: return SDL_SCANCODE_X;
    case KEY_Y: return SDL_SCANCODE_Y;
    case KEY_Z: return SDL_SCANCODE_Z;
    case KEY_ONE: return SDL_SCANCODE_1;
    case KEY_TWO: return SDL_SCANCODE_2;
    case KEY_THREE: return SDL_SCANCODE_3;
    case KEY_FOUR: return SDL_SCANCODE_4;
    case KEY_FIVE: return SDL_SCANCODE_5;
    case KEY_SIX: return SDL_SCANCODE_6;
    case KEY_SEVEN: return SDL_SCANCODE_7;
    case KEY_EIGHT: return SDL_SCANCODE_8;
    case KEY_NINE: return SDL_SCANCODE_9;
    case KEY_ZERO: return SDL_SCANCODE_0;
    case KEY_SPACE: return SDL_SCANCODE_SPACE;
    case KEY_MINUS: return SDL_SCANCODE_MINUS;
    case KEY_EQUAL: return SDL_SCANCODE_EQUALS;
    case KEY_LEFT_BRACKET: return SDL_SCANCODE_LEFTBRACKET;
    case KEY_RIGHT_BRACKET: return SDL_SCANCODE_RIGHTBRACKET;
    case KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
    case KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
    case KEY_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
    case KEY_GRAVE: return SDL_SCANCODE_GRAVE;
    case KEY_COMMA: return SDL_SCANCODE_COMMA;
    case KEY_PERIOD: return SDL_SCANCODE_PERIOD;
    case KEY_SLASH: return SDL_SCANCODE_SLASH;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static int k_layout_codepoint_from_name(const char *name, int fallback)
{
    int bytes = 0;
    int codepoint;

    if(name == NULL || name[0] == '\0')
        return fallback;
    codepoint = GetCodepointNext(name, &bytes);
    if(bytes <= 0 || name[bytes] != '\0')
        return fallback;
    return k_layout_codepoint(codepoint);
}

static int k_sdl_layout_codepoint(int key, int fallback)
{
    SDL_Scancode scancode = k_sdl_scancode_for_key(key);
    SDL_Keycode keycode;

    if(scancode == SDL_SCANCODE_UNKNOWN)
        return fallback;
    keycode = SDL_GetKeyFromScancode(scancode);
    if(keycode == SDLK_UNKNOWN)
        return fallback;
    return k_layout_codepoint_from_name(SDL_GetKeyName(keycode), fallback);
}
#endif

/* Buttons and wheel are hidden from the backend while an override window
 * owns the pointer from outside or swallows buttons itself. */
static int k_input_override_blocks_buttons(void)
{
    return g_kryon_input_override.enabled &&
           (!g_kryon_input_override.mouse_inside ||
            !g_kryon_input_override.pass_buttons);
}

static int k_input_override_blocks_keyboard(void)
{
    return g_kryon_input_override.enabled &&
           !g_kryon_input_override.pass_keyboard;
}

void BeginKryonInputOverride(KryonInputOverride input)
{
    if(g_kryon_input_override_depth < KRYON_INPUT_OVERRIDE_STACK_CAP)
        g_kryon_input_override_stack[g_kryon_input_override_depth++] =
            g_kryon_input_override;
    input.enabled = 1;
    g_kryon_input_override = input;
}

void EndKryonInputOverride(void)
{
    if(g_kryon_input_override_depth > 0) {
        g_kryon_input_override =
            g_kryon_input_override_stack[--g_kryon_input_override_depth];
        return;
    }
    g_kryon_input_override = kryon_zero_kryoninputoverride;
}

int SetKeyboardInputEnabled(int enabled)
{
    int old = g_kryon_keyboard_input_enabled;

    g_kryon_keyboard_input_enabled = enabled != 0;
    return old;
}

int KeyboardInputEnabled(void)
{
    return g_kryon_keyboard_input_enabled;
}

void SetKeyPlatformCallbacks(KeyInputPlatformCallback update,
                             KeyPlatformCallback key_pressed,
                             KeyPlatformCallback key_down)
{
    g_kryon_key_input_update_callback = update;
    g_kryon_key_pressed_callback = key_pressed;
    g_kryon_key_down_callback = key_down;
}

void UpdateKeyPlatformState(void)
{
    InjectPump();
    if(g_kryon_key_input_update_callback != NULL)
        g_kryon_key_input_update_callback();
}

int GetLayoutKeyCodepoint(int key)
{
#ifndef KRYON_BACKEND_RAYLIB
    const char *name;
    int bytes = 0;
    int codepoint;
#endif
    int fallback = 0;

    if(key <= 0)
        return 0;

    if(key == KEY_SPACE)
        fallback = ' ';
    else if(key >= KEY_APOSTROPHE && key <= KEY_GRAVE)
        fallback = k_layout_codepoint(key);

#ifdef KRYON_BACKEND_RAYLIB
    return k_sdl_layout_codepoint(key, fallback);
#else
    name = GetKeyName(key);
    if(name == NULL || name[0] == '\0')
        return fallback;
    codepoint = GetCodepointNext(name, &bytes);
    if(bytes <= 0 || name[bytes] != '\0')
        return fallback;
    return k_layout_codepoint(codepoint);
#endif
}

bool IsLayoutKeyPressed(int codepoint)
{
    codepoint = k_layout_codepoint(codepoint);
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectLayoutKeyPressed(codepoint))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    for(int key = KEY_SPACE; key <= KEY_GRAVE; key++) {
        if(k_layout_key_matches(key, codepoint) && IsKeyPressed(key))
            return true;
    }
    return false;
}

bool IsLayoutKeyPressedRepeat(int codepoint)
{
    codepoint = k_layout_codepoint(codepoint);
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectLayoutKeyPressed(codepoint))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    for(int key = KEY_SPACE; key <= KEY_GRAVE; key++) {
        if(k_layout_key_matches(key, codepoint) && IsKeyPressedRepeat(key))
            return true;
    }
    return false;
}

bool IsLayoutKeyDown(int codepoint)
{
    codepoint = k_layout_codepoint(codepoint);
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectLayoutKeyDown(codepoint))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    for(int key = KEY_SPACE; key <= KEY_GRAVE; key++) {
        if(k_layout_key_matches(key, codepoint) && IsKeyDown(key))
            return true;
    }
    return false;
}

bool IsLayoutKeyReleased(int codepoint)
{
    codepoint = k_layout_codepoint(codepoint);
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectLayoutKeyReleased(codepoint))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    for(int key = KEY_SPACE; key <= KEY_GRAVE; key++) {
        if(k_layout_key_matches(key, codepoint) && IsKeyReleased(key))
            return true;
    }
    return false;
}

bool IsKeyPressed(int key)
{
    KeyPlatformCallback pressed = g_kryon_key_pressed_callback;

    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectKeyPressed(key))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    if(pressed != NULL && k_key_prefers_platform(key))
        return pressed(key);
    if(BackendRaw_IsKeyPressed(key))
        return true;
    return pressed != NULL && pressed(key);
}

bool IsKeyPressedRepeat(int key)
{
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectKeyPressed(key))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    return BackendRaw_IsKeyPressedRepeat(key);
}

bool IsKeyDown(int key)
{
    KeyPlatformCallback down = g_kryon_key_down_callback;

    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectKeyDown(key))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    if(down != NULL && k_key_prefers_platform(key))
        return down(key);
    if(BackendRaw_IsKeyDown(key))
        return true;
    return down != NULL && down(key);
}

bool IsKeyReleased(int key)
{
    if(k_input_override_blocks_keyboard())
        return false;
    if(InjectKeyReleased(key))
        return true;
    return BackendRaw_IsKeyReleased(key);
}

int GetKeyPressed(void)
{
    int injected;

    if(k_input_override_blocks_keyboard())
        return 0;
    injected = InjectKeyPressedCode();
    if(injected != 0)
        return injected;
    return BackendRaw_GetKeyPressed();
}

int GetCharPressed(void)
{
    int injected;

    if(k_input_override_blocks_keyboard())
        return 0;
    injected = InjectCharPressed();
    if(injected != 0)
        return injected;
    return BackendRaw_GetCharPressed();
}

Vector2
kry_mouse_press_position(Vector2 fallback)
{
#ifdef KRYON_BACKEND_RAYLIB
    if(sdl_frame_press[MOUSE_BUTTON_LEFT] && !InjectMouseActive() &&
       !g_kryon_input_override.enabled)
        return sdl_frame_press_position;
#endif
    return fallback;
}

bool IsMouseButtonPressed(int button)
{
    if(InjectMousePressed(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
#if ANDROID_BUILD
    if(button == MOUSE_BUTTON_LEFT && android_frame_press)
        return true;
#endif
#ifdef KRYON_BACKEND_RAYLIB
    if(button >= 0 && button < 8 && sdl_frame_press[button])
        return true;
#endif
    return BackendRaw_IsMouseButtonPressed(button);
}

bool IsMouseButtonDown(int button)
{
    if(InjectMouseButtonDown(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
    return BackendRaw_IsMouseButtonDown(button);
}

bool IsMouseButtonReleased(int button)
{
    if(InjectMouseReleased(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
#if ANDROID_BUILD
    if(button == MOUSE_BUTTON_LEFT) {
        if(android_frame_cancel)
            return false;
        if(android_frame_release)
            return true;
    }
#endif
#ifdef KRYON_BACKEND_RAYLIB
    if(button >= 0 && button < 8 && sdl_frame_release[button])
        return true;
#endif
    return BackendRaw_IsMouseButtonReleased(button);
}

bool IsMouseButtonUp(int button)
{
    if(InjectMouseButtonUp(button))
        return true;
    if(k_input_override_blocks_buttons())
        return true;
    return BackendRaw_IsMouseButtonUp(button);
}

int GetMouseX(void)
{
    if(InjectMouseActive())
        return (int)InjectMouseX();
    if(g_kryon_input_override.enabled)
        return (int)g_kryon_input_override.mouse_position.x;
    return BackendRaw_GetMouseX();
}

int GetMouseY(void)
{
    if(InjectMouseActive())
        return (int)InjectMouseY();
    if(g_kryon_input_override.enabled)
        return (int)g_kryon_input_override.mouse_position.y;
    return BackendRaw_GetMouseY();
}

Vector2 GetMousePosition(void)
{
    if(InjectMouseActive()) {
        Vector2 injected = {InjectMouseX(), InjectMouseY()};
        return injected;
    }
    if(g_kryon_input_override.enabled)
        return g_kryon_input_override.mouse_position;
    return BackendRaw_GetMousePosition();
}

Vector2 GetMouseDelta(void)
{
    if(InjectMouseActive()) {
        Vector2 injected = {InjectMouseDeltaX(), InjectMouseDeltaY()};
        return injected;
    }
    if(g_kryon_input_override.enabled)
        return g_kryon_input_override.mouse_delta;
    return BackendRaw_GetMouseDelta();
}

float GetMouseWheelMove(void)
{
    if(InjectWheelValue() != 0.0f)
        return InjectWheelValue();
    if(k_input_override_blocks_buttons())
        return 0.0f;
    return BackendRaw_GetMouseWheelMove();
}

Vector2 GetMouseWheelMoveV(void)
{
    if(k_input_override_blocks_buttons())
        return (Vector2){0.0f, 0.0f};
    return BackendRaw_GetMouseWheelMoveV();
}
