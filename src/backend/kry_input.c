/* Shared input front-end; see kry_input.h. The public input queries merge
 * synthetic injection, the modal input override, and the keyboard platform
 * callbacks around the active backends BackendRaw_* hooks, so every
 * backend gets identical input behavior. Previously this logic was emitted
 * only into the generated raylib wrappers (tools/generate-kryon-compat.sh). */

#include "kry_input.h"
#include "kry_inject.h"
#include "app_host.h"

#include <stddef.h>

#ifdef KRYON_BACKEND_RAYLIB
#include <SDL.h>
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

bool IsMouseButtonPressed(int button)
{
    if(InjectMousePressed(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
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
