/*
 * canvas_input.c — input queries over the JS event queues.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * kry_input.h routes the kryon input layer through these
 * KryonBackendRaw_* hooks; js_input_query both reads and (for the
 * clear-opcodes) resets the one-frame edges.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

/* ------------------------------------------------------------------ */
/* Input queries over the JS queues (which also clears edges)         */
/* ------------------------------------------------------------------ */

EM_JS(int, js_input_query, (int which, int code), {
    var K = globalThis.__kryCanvas;
    if (!K) return 0;
    switch (which) {
    case 0: return K.keysDown[code] ? 1 : 0;
    case 1: return K.keysPressed.indexOf(code) >= 0 ? 1 : 0;
    case 2: return K.keysReleased.indexOf(code) >= 0 ? 1 : 0;
    case 3: return K.keysPressed.length > 0 ? K.keysPressed.shift() : 0;
    case 4: return K.chars.length > 0 ? K.chars.shift() : 0;
    case 5: return K.buttonsDown[code] ? 1 : 0;
    case 6: return K.buttonsPressed.indexOf(code) >= 0 ? 1 : 0;
    case 7: return K.buttonsReleased.indexOf(code) >= 0 ? 1 : 0;
    case 8: return K.mouseX;
    case 9: return K.mouseY;
    case 10: return K.mouseDeltaX;
    case 11: return K.mouseDeltaY;
    case 12: { var w = K.wheel; return w < 0 ? -1 : (w > 0 ? 1 : 0); }
    case 13: K.keysPressed = []; return 1;
    case 14: K.keysReleased = []; return 1;
    case 15: K.buttonsPressed = []; return 1;
    case 16: K.buttonsReleased = []; return 1;
    case 17: K.mouseDeltaX = 0; K.mouseDeltaY = 0; return 1;
    case 18: K.wheel = 0; return 1;
    }
    return 0;
});

bool KryonBackendRaw_IsKeyPressed(int key)
{
    return js_input_query(1, key) != 0;
}

bool KryonBackendRaw_IsKeyPressedRepeat(int key)
{
    return KryonBackendRaw_IsKeyPressed(key);
}

bool KryonBackendRaw_IsKeyDown(int key)
{
    return js_input_query(0, key) != 0;
}

bool KryonBackendRaw_IsKeyReleased(int key)
{
    return js_input_query(2, key) != 0;
}

int KryonBackendRaw_GetKeyPressed(void)
{
    return js_input_query(3, 0);
}

int KryonBackendRaw_GetCharPressed(void)
{
    return js_input_query(4, 0);
}

bool KryonBackendRaw_IsMouseButtonPressed(int button)
{
    return js_input_query(6, button) != 0;
}

bool KryonBackendRaw_IsMouseButtonDown(int button)
{
    return js_input_query(5, button) != 0;
}

bool KryonBackendRaw_IsMouseButtonReleased(int button)
{
    return js_input_query(7, button) != 0;
}

bool KryonBackendRaw_IsMouseButtonUp(int button)
{
    return js_input_query(5, button) == 0;
}

int KryonBackendRaw_GetMouseX(void)
{
    return js_input_query(8, 0);
}

int KryonBackendRaw_GetMouseY(void)
{
    return js_input_query(9, 0);
}

Vector2 KryonBackendRaw_GetMousePosition(void)
{
    return (Vector2){(float)js_input_query(8, 0),
                     (float)js_input_query(9, 0)};
}

Vector2 KryonBackendRaw_GetMouseDelta(void)
{
    return (Vector2){(float)js_input_query(10, 0),
                     (float)js_input_query(11, 0)};
}

float KryonBackendRaw_GetMouseWheelMove(void)
{
    return (float)js_input_query(12, 0);
}

Vector2 KryonBackendRaw_GetMouseWheelMoveV(void)
{
    return (Vector2){0.0f, (float)js_input_query(12, 0)};
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
