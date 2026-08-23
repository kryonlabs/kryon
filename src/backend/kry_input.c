/* Shared input front-end; see kry_input.h. The public input queries merge
 * synthetic injection, the modal input override, and the keyboard platform
 * callbacks around the active backends KryonBackendRaw_* hooks, so every
 * backend gets identical input behavior. Previously this logic was emitted
 * only into the generated raylib wrappers (tools/generate-kryon-compat.sh). */

#include "kry_input.h"
#include "kry_inject.h"
#include "app_host.h"

#include <stddef.h>

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

/* Buttons and wheel are hidden from the backend while an override window
 * owns the pointer from outside or swallows buttons itself. */
static int k_input_override_blocks_buttons(void)
{
    return g_kryon_input_override.enabled &&
           (!g_kryon_input_override.mouse_inside ||
            !g_kryon_input_override.pass_buttons);
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
    g_kryon_input_override = (KryonInputOverride){0};
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
    KryonInjectPump();
    if(g_kryon_key_input_update_callback != NULL)
        g_kryon_key_input_update_callback();
}

bool IsKeyPressed(int key)
{
    KeyPlatformCallback pressed = g_kryon_key_pressed_callback;

    if(KryonInjectKeyPressed(key))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    if(pressed != NULL && k_key_prefers_platform(key))
        return pressed(key);
    if(KryonBackendRaw_IsKeyPressed(key))
        return true;
    return pressed != NULL && pressed(key);
}

bool IsKeyDown(int key)
{
    KeyPlatformCallback down = g_kryon_key_down_callback;

    if(KryonInjectKeyDown(key))
        return true;
    if(!g_kryon_keyboard_input_enabled)
        return false;
    if(down != NULL && k_key_prefers_platform(key))
        return down(key);
    if(KryonBackendRaw_IsKeyDown(key))
        return true;
    return down != NULL && down(key);
}

bool IsKeyReleased(int key)
{
    if(KryonInjectKeyReleased(key))
        return true;
    return KryonBackendRaw_IsKeyReleased(key);
}

int GetKeyPressed(void)
{
    int injected = KryonInjectKeyPressedCode();
    if(injected != 0)
        return injected;
    return KryonBackendRaw_GetKeyPressed();
}

int GetCharPressed(void)
{
    int injected = KryonInjectCharPressed();
    if(injected != 0)
        return injected;
    return KryonBackendRaw_GetCharPressed();
}

bool IsMouseButtonPressed(int button)
{
    if(KryonInjectMousePressed(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
    return KryonBackendRaw_IsMouseButtonPressed(button);
}

bool IsMouseButtonDown(int button)
{
    if(KryonInjectMouseButtonDown(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
    return KryonBackendRaw_IsMouseButtonDown(button);
}

bool IsMouseButtonReleased(int button)
{
    if(KryonInjectMouseReleased(button))
        return true;
    if(k_input_override_blocks_buttons())
        return false;
    return KryonBackendRaw_IsMouseButtonReleased(button);
}

bool IsMouseButtonUp(int button)
{
    if(KryonInjectMouseButtonUp(button))
        return true;
    if(k_input_override_blocks_buttons())
        return true;
    return KryonBackendRaw_IsMouseButtonUp(button);
}

int GetMouseX(void)
{
    if(KryonInjectMouseActive())
        return (int)KryonInjectMouseX();
    if(g_kryon_input_override.enabled)
        return (int)g_kryon_input_override.mouse_position.x;
    return KryonBackendRaw_GetMouseX();
}

int GetMouseY(void)
{
    if(KryonInjectMouseActive())
        return (int)KryonInjectMouseY();
    if(g_kryon_input_override.enabled)
        return (int)g_kryon_input_override.mouse_position.y;
    return KryonBackendRaw_GetMouseY();
}

Vector2 GetMousePosition(void)
{
    if(KryonInjectMouseActive()) {
        Vector2 injected = {KryonInjectMouseX(), KryonInjectMouseY()};
        return injected;
    }
    if(g_kryon_input_override.enabled)
        return g_kryon_input_override.mouse_position;
    return KryonBackendRaw_GetMousePosition();
}

Vector2 GetMouseDelta(void)
{
    if(KryonInjectMouseActive()) {
        Vector2 injected = {KryonInjectMouseDeltaX(), KryonInjectMouseDeltaY()};
        return injected;
    }
    if(g_kryon_input_override.enabled)
        return g_kryon_input_override.mouse_delta;
    return KryonBackendRaw_GetMouseDelta();
}

float GetMouseWheelMove(void)
{
    if(KryonInjectWheelValue() != 0.0f)
        return KryonInjectWheelValue();
    if(k_input_override_blocks_buttons())
        return 0.0f;
    return KryonBackendRaw_GetMouseWheelMove();
}

Vector2 GetMouseWheelMoveV(void)
{
    if(k_input_override_blocks_buttons())
        return (Vector2){0.0f, 0.0f};
    return KryonBackendRaw_GetMouseWheelMoveV();
}
