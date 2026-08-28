/*
 * canvas_input.c — input queries over the JS event queues.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * kry_input.h routes the kryon input layer through these
 * BackendRaw_* hooks; js_input_query both reads and (for the
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
    case 12: {
        var w = Math.abs(K.wheelX) > Math.abs(K.wheelY) ? K.wheelX : K.wheelY;
        return w < 0 ? Math.floor(w) : Math.ceil(w);
    }
    case 13: K.keysPressed = []; return 1;
    case 14: K.keysReleased = []; return 1;
    case 15: K.buttonsPressed = []; return 1;
    case 16: K.buttonsReleased = []; return 1;
    case 17: K.mouseDeltaX = 0; K.mouseDeltaY = 0; return 1;
    case 18: K.wheelX = 0; K.wheelY = 0; return 1;
    case 19: return K.keysRepeated.indexOf(code) >= 0 ? 1 : 0;
    case 20: K.keysRepeated = []; return 1;
    }
    return 0;
});

EM_JS(double, js_input_float, (int which), {
    var K = globalThis.__kryCanvas;
    if (!K) return 0.0;
    switch (which) {
    case 0: return K.mouseDeltaX;
    case 1: return K.mouseDeltaY;
    case 2: return K.wheelX;
    case 3: return K.wheelY;
    case 4: return Math.abs(K.wheelX) > Math.abs(K.wheelY) ? K.wheelX : K.wheelY;
    }
    return 0.0;
});

EM_JS(void, js_input_set_mouse, (int x, int y), {
    var K = globalThis.__kryCanvas;
    if (!K) return;
    K.mouseDeltaX += x - K.mouseX;
    K.mouseDeltaY += y - K.mouseY;
    K.mouseX = x;
    K.mouseY = y;
});

EM_JS(void, js_input_mouse_config, (double ox, double oy,
                                    double sx, double sy), {
    var K = globalThis.__kryCanvas;
    if (!K) return;
    K.mouseOffsetX = ox;
    K.mouseOffsetY = oy;
    K.mouseScaleX = sx;
    K.mouseScaleY = sy;
});

EM_JS(int, js_touch_query, (int index, int field), {
    var K = globalThis.__kryCanvas;
    if (!K || index < 0 || index >= K.touches.length) return 0;
    var t = K.touches[index];
    switch (field) {
    case 0: return t.x | 0;
    case 1: return t.y | 0;
    case 2: return t.id | 0;
    case 3: return K.touches.length | 0;
    }
    return 0;
});

EM_JS(int, js_gamepad_query, (int gamepad, int which, int index), {
    var K = globalThis.__kryCanvas;
    if (!K) return 0;
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    if (!pad) return 0;
    var mapButton = function (button) {
        switch (button) {
        case 1: return 12; /* dpad up */
        case 2: return 15; /* dpad right */
        case 3: return 13; /* dpad down */
        case 4: return 14; /* dpad left */
        case 5: return 3;  /* Y/triangle */
        case 6: return 1;  /* B/circle */
        case 7: return 0;  /* A/cross */
        case 8: return 2;  /* X/square */
        case 9: return 4;
        case 10: return 6;
        case 11: return 5;
        case 12: return 7;
        case 13: return 8;
        case 14: return 16;
        case 15: return 9;
        case 16: return 10;
        case 17: return 11;
        default: return -1;
        }
    };
    var buttons = pad.buttons || [];
    var now = [];
    for (var i = 0; i < buttons.length; i++)
        now[i] = !!(buttons[i] && buttons[i].pressed);
    var prev = K.gamepadPrev[gamepad] || [];
    K.gamepadNow[gamepad] = now;
    if (!K.gamepadPressed[gamepad]) K.gamepadPressed[gamepad] = [];
    if (!K.gamepadReleased[gamepad]) K.gamepadReleased[gamepad] = [];
    K.gamepadPressed[gamepad] = [];
    K.gamepadReleased[gamepad] = [];
    for (var b = 0; b < now.length; b++) {
        if (now[b] && !prev[b]) {
            K.gamepadPressed[gamepad].push(b);
            K.lastGamepadButton = b;
        } else if (!now[b] && prev[b]) {
            K.gamepadReleased[gamepad].push(b);
        }
    }
    if (which === 0) return 1;
    var browserButton = mapButton(index);
    if (which === 1) return browserButton >= 0 && K.gamepadPressed[gamepad].indexOf(browserButton) >= 0 ? 1 : 0;
    if (which === 2) return browserButton >= 0 && now[browserButton] ? 1 : 0;
    if (which === 3) return browserButton >= 0 && K.gamepadReleased[gamepad].indexOf(browserButton) >= 0 ? 1 : 0;
    if (which === 4) {
        switch (K.lastGamepadButton) {
        case 12: return 1;
        case 15: return 2;
        case 13: return 3;
        case 14: return 4;
        case 3: return 5;
        case 1: return 6;
        case 0: return 7;
        case 2: return 8;
        case 4: return 9;
        case 6: return 10;
        case 5: return 11;
        case 7: return 12;
        case 8: return 13;
        case 16: return 14;
        case 9: return 15;
        case 10: return 16;
        case 11: return 17;
        default: return 0;
        }
    }
    if (which === 5) return pad.axes ? pad.axes.length : 0;
    return 0;
});

EM_JS(double, js_gamepad_axis, (int gamepad, int axis), {
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    if (!pad) return 0.0;
    if (axis >= 0 && axis < 4 && pad.axes && axis < pad.axes.length)
        return pad.axes[axis] || 0.0;
    if (axis === 4 && pad.buttons && pad.buttons[6])
        return (pad.buttons[6].value || 0.0) * 2.0 - 1.0;
    if (axis === 5 && pad.buttons && pad.buttons[7])
        return (pad.buttons[7].value || 0.0) * 2.0 - 1.0;
    return 0.0;
});

EM_JS(char *, js_gamepad_name, (int gamepad), {
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    var name = pad && pad.id ? pad.id : "";
    var len = lengthBytesUTF8(name) + 1;
    var ptr = _malloc(len);
    stringToUTF8(name, ptr, len);
    return ptr;
});

EM_JS(void, js_input_end_frame, (void), {
    var K = globalThis.__kryCanvas;
    if (!K) return;
    for (var i = 0; i < K.gamepadNow.length; i++)
        if (K.gamepadNow[i]) K.gamepadPrev[i] = K.gamepadNow[i].slice();
});

bool BackendRaw_IsKeyPressed(int key)
{
    return js_input_query(1, key) != 0;
}

bool BackendRaw_IsKeyPressedRepeat(int key)
{
    return js_input_query(19, key) != 0;
}

bool BackendRaw_IsKeyDown(int key)
{
    return js_input_query(0, key) != 0;
}

bool BackendRaw_IsKeyReleased(int key)
{
    return js_input_query(2, key) != 0;
}

int BackendRaw_GetKeyPressed(void)
{
    return js_input_query(3, 0);
}

int BackendRaw_GetCharPressed(void)
{
    return js_input_query(4, 0);
}

bool BackendRaw_IsMouseButtonPressed(int button)
{
    return js_input_query(6, button) != 0;
}

bool BackendRaw_IsMouseButtonDown(int button)
{
    return js_input_query(5, button) != 0;
}

bool BackendRaw_IsMouseButtonReleased(int button)
{
    return js_input_query(7, button) != 0;
}

bool BackendRaw_IsMouseButtonUp(int button)
{
    return js_input_query(5, button) == 0;
}

int BackendRaw_GetMouseX(void)
{
    return js_input_query(8, 0);
}

int BackendRaw_GetMouseY(void)
{
    return js_input_query(9, 0);
}

Vector2 BackendRaw_GetMousePosition(void)
{
    return (Vector2){(float)js_input_query(8, 0),
                     (float)js_input_query(9, 0)};
}

Vector2 BackendRaw_GetMouseDelta(void)
{
    return (Vector2){(float)js_input_float(0),
                     (float)js_input_float(1)};
}

float BackendRaw_GetMouseWheelMove(void)
{
    return (float)js_input_float(4);
}

Vector2 BackendRaw_GetMouseWheelMoveV(void)
{
    return (Vector2){(float)js_input_float(2), (float)js_input_float(3)};
}

void SetMousePosition(int x, int y)
{
    js_input_set_mouse(x, y);
}

void SetMouseOffset(int offsetX, int offsetY)
{
    EM_ASM({
        var K = globalThis.__kryCanvas;
        if (K) {
            K.mouseOffsetX = $0;
            K.mouseOffsetY = $1;
        }
    }, offsetX, offsetY);
}

void SetMouseScale(float scaleX, float scaleY)
{
    EM_ASM({
        var K = globalThis.__kryCanvas;
        if (K) {
            K.mouseScaleX = $0;
            K.mouseScaleY = $1;
        }
    }, scaleX, scaleY);
}

int GetTouchX(void)
{
    return js_touch_query(0, 0);
}

int GetTouchY(void)
{
    return js_touch_query(0, 1);
}

Vector2 GetTouchPosition(int index)
{
    return (Vector2){(float)js_touch_query(index, 0),
                     (float)js_touch_query(index, 1)};
}

int GetTouchPointId(int index)
{
    return js_touch_query(index, 2);
}

int GetTouchPointCount(void)
{
    return js_touch_query(0, 3);
}

bool IsGamepadAvailable(int gamepad)
{
    return js_gamepad_query(gamepad, 0, 0) != 0;
}

const char *GetGamepadName(int gamepad)
{
    static char names[4][128];
    static int next;
    char *tmp = js_gamepad_name(gamepad);
    char *dst = names[next++ % 4];

    snprintf(dst, sizeof(names[0]), "%s", tmp != NULL ? tmp : "");
    free(tmp);
    return dst;
}

bool IsGamepadButtonPressed(int gamepad, int button)
{
    return js_gamepad_query(gamepad, 1, button) != 0;
}

bool IsGamepadButtonDown(int gamepad, int button)
{
    return js_gamepad_query(gamepad, 2, button) != 0;
}

bool IsGamepadButtonReleased(int gamepad, int button)
{
    return js_gamepad_query(gamepad, 3, button) != 0;
}

bool IsGamepadButtonUp(int gamepad, int button)
{
    return !IsGamepadButtonDown(gamepad, button);
}

int GetGamepadButtonPressed(void)
{
    return js_gamepad_query(0, 4, 0);
}

int GetGamepadAxisCount(int gamepad)
{
    return js_gamepad_query(gamepad, 5, 0);
}

float GetGamepadAxisMovement(int gamepad, int axis)
{
    return (float)js_gamepad_axis(gamepad, axis);
}

int SetGamepadMappings(const char *mappings)
{
    (void)mappings;
    return 0;
}

void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor,
                         float duration)
{
    (void)gamepad;
    (void)leftMotor;
    (void)rightMotor;
    (void)duration;
}

const char *GetKeyName(int key)
{
    static char name[16];

    if(key >= 32 && key <= 126) {
        name[0] = (char)key;
        name[1] = '\0';
        return name;
    }
    switch(key) {
    case KEY_SPACE: return "space";
    case KEY_ESCAPE: return "escape";
    case KEY_ENTER: return "enter";
    case KEY_TAB: return "tab";
    case KEY_BACKSPACE: return "backspace";
    case KEY_INSERT: return "insert";
    case KEY_DELETE: return "delete";
    case KEY_RIGHT: return "right";
    case KEY_LEFT: return "left";
    case KEY_DOWN: return "down";
    case KEY_UP: return "up";
    case KEY_PAGE_UP: return "page up";
    case KEY_PAGE_DOWN: return "page down";
    case KEY_HOME: return "home";
    case KEY_END: return "end";
    default:
        if(key >= KEY_F1 && key <= KEY_F12) {
            snprintf(name, sizeof(name), "f%d", key - KEY_F1 + 1);
            return name;
        }
        return "";
    }
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
