/*
 * canvas_window.c — window/frame lifecycle, canvas boot, ctx dispatcher.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * Implements the window and frame surface against a 2d canvas context
 * through EM_JS, plus the KryonRaylibBackend_* seam kry_instance.c and
 * kry_screenshot.c forward to.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

/* ------------------------------------------------------------------ */
/* JS side: canvas, event queues, draw dispatcher                     */
/* ------------------------------------------------------------------ */

EM_JS(void, js_canvas_boot, (int w, int h), {
    var g = globalThis;
    var K = g.__kryCanvas = {
        w: w, h: h,
        canvas: null, ctx: null,
        textures: {}, nextTex: 1,
        target: [],               /* render-target stack: {canvas,ctx} */
        saved: 0,                 /* outstanding ctx.save() pairs */
        keysDown: {}, keysPressed: [], keysReleased: [], chars: [],
        mouseX: 0, mouseY: 0, mouseDeltaX: 0, mouseDeltaY: 0,
        buttonsDown: {}, buttonsPressed: [], buttonsReleased: [],
        wheel: 0, frames: 0, lastOp: 'boot'
    };
    var doc = (typeof document !== 'undefined') ? document : null;
    if (doc) {
        K.canvas = doc.getElementById('canvas');
        if (!K.canvas) {
            K.canvas = doc.createElement('canvas');
            doc.body.appendChild(K.canvas);
        }
    } else if (g.__kryTestCanvas) {
        K.canvas = g.__kryTestCanvas;    /* node test harness */
    }
    if (K.canvas) {
        K.canvas.width = w; K.canvas.height = h;
        K.ctx = K.canvas.getContext('2d');
    }
    var KEYMAP = {
        Space: 32, Escape: 256, Enter: 257, NumpadEnter: 257, Tab: 258,
        Backspace: 259, Insert: 260, Delete: 261, ArrowRight: 262,
        ArrowLeft: 263, ArrowDown: 264, ArrowUp: 265, PageUp: 266,
        PageDown: 267, Home: 268, End: 269, CapsLock: 280, ScrollLock: 281,
        NumLock: 282, PrintScreen: 283, Pause: 284, F1: 290, F2: 291,
        F3: 292, F4: 293, F5: 294, F6: 295, F7: 296, F8: 297, F9: 298,
        F10: 299, F11: 300, F12: 301, ShiftLeft: 340, ShiftRight: 344,
        ControlLeft: 341, ControlRight: 345, AltLeft: 342, AltRight: 346,
        MetaLeft: 346, MetaRight: 347, Semicolon: 59, Equal: 61, Comma: 44,
        Minus: 45, Period: 46, Slash: 47, Backquote: 96, BracketLeft: 91,
        Backslash: 92, BracketRight: 93, Quote: 39, Digit0: 48, Digit1: 49,
        Digit2: 50, Digit3: 51, Digit4: 52, Digit5: 53, Digit6: 54,
        Digit7: 55, Digit8: 56, Digit9: 57
    };
    var keyOf = function (code) {
        if (code.startsWith('Key') && code.length === 4) {
            var c = code.charCodeAt(3);
            if (c >= 65 && c <= 90) return c;
        }
        if (code.startsWith('Numpad') && code.length === 7) {
            var d = code.charCodeAt(6);
            if (d >= 48 && d <= 57) return d;
        }
        return KEYMAP[code] !== undefined ? KEYMAP[code] : 0;
    };
    var hook = function (target) {
        if (!target || target.__kryHooked || !target.addEventListener)
            return;
        target.__kryHooked = 1;
        target.addEventListener('mousemove', function (e) {
            var r = K.canvas ? K.canvas.getBoundingClientRect()
                            : {left: 0, top: 0};
            var nx = e.clientX - r.left, ny = e.clientY - r.top;
            K.mouseDeltaX += nx - K.mouseX;
            K.mouseDeltaY += ny - K.mouseY;
            K.mouseX = nx; K.mouseY = ny;
        });
        target.addEventListener('mousedown', function (e) {
            K.buttonsDown[e.button] = 1;
            K.buttonsPressed.push(e.button);
        });
        target.addEventListener('mouseup', function (e) {
            delete K.buttonsDown[e.button];
            K.buttonsReleased.push(e.button);
        });
        target.addEventListener('wheel', function (e) {
            K.wheel += e.deltaY > 0 ? -1.0 : 1.0;
        });
        target.addEventListener('keydown', function (e) {
            var k = keyOf(e.code);
            if (k) {
                if (!K.keysDown[k]) K.keysPressed.push(k);
                K.keysDown[k] = 1;
            }
        });
        target.addEventListener('keyup', function (e) {
            var k = keyOf(e.code);
            if (k) { delete K.keysDown[k]; K.keysReleased.push(k); }
        });
        target.addEventListener('keypress', function (e) {
            if (e.key && e.key.length === 1)
                K.chars.push(e.key.charCodeAt(0));
        });
    };
    if (doc) hook(doc);
    hook(g);
});

EM_JS(void, js_canvas_resize, (int w, int h), {
    var K = globalThis.__kryCanvas;
    K.w = w; K.h = h;
    if (K.canvas) { K.canvas.width = w; K.canvas.height = h; }
});

EM_JS(void, js_ctx_call, (int op, double a, double b, double c, double d,
                          double e, double f, double g2,
                          int r, int gg, int bb, int aa),
{
    var K = globalThis.__kryCanvas;
    var ctx = K.target.length ? K.target[K.target.length - 1].ctx : K.ctx;
    K.lastOp = 'ctx' + op;
    if (!ctx) return;
    var col = 'rgba(' + r + ',' + gg + ',' + bb + ',' + (aa / 255.0) + ')';
    switch (op) {
    case 0: /* clear */ ctx.fillStyle = col;
            ctx.fillRect(0, 0, K.w, K.h); break;
    case 1: /* fill rect */ ctx.fillStyle = col;
            ctx.fillRect(a, b, c, d); break;
    case 2: /* stroke rect */ ctx.strokeStyle = col; ctx.lineWidth = 1;
            ctx.strokeRect(a + .5, b + .5, c - 1, d - 1); break;
    case 3: /* fill circle */ ctx.fillStyle = col; ctx.beginPath();
            ctx.arc(a, b, c, 0, 6.28318530718, false); ctx.fill(); break;
    case 4: /* stroke circle */ ctx.strokeStyle = col; ctx.lineWidth = 1;
            ctx.beginPath(); ctx.arc(a, b, c, 0, 6.28318530718, false);
            ctx.stroke(); break;
    case 5: /* line */ ctx.strokeStyle = col; ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(a + .5, b + .5);
            ctx.lineTo(c + .5, d + .5); ctx.stroke(); break;
    case 6: /* thick line */ ctx.strokeStyle = col; ctx.lineWidth = e;
            ctx.lineCap = 'round'; ctx.beginPath(); ctx.moveTo(a, b);
            ctx.lineTo(c, d); ctx.stroke(); break;
    case 7: /* triangle */ ctx.fillStyle = col; ctx.beginPath();
            ctx.moveTo(a, b); ctx.lineTo(c, d); ctx.lineTo(e, f);
            ctx.closePath(); ctx.fill(); break;
    case 9: /* scissor push */ ctx.save(); ctx.beginPath();
            ctx.rect(a, b, c, d); ctx.clip(); K.saved++; break;
    case 10: /* scissor pop */ if (K.saved > 0) { ctx.restore(); K.saved--; }
             break;
    case 11: /* mode2d push: raylib camera transform */
             ctx.save();
             ctx.translate(c, d);          /* offset */
             ctx.rotate(-g2 * Math.PI / 180.0);
             ctx.scale(a, b);              /* zoom */
             ctx.translate(-e, -f);        /* -target */
             break;
    case 12: /* mode2d pop */ ctx.restore(); break;
    case 13: /* frame reset: identity transform */
             ctx.setTransform(1, 0, 0, 1, 0, 0);
             K.saved = 0;
             break;
    }
});

/* ------------------------------------------------------------------ */
/* C state                                                            */
/* ------------------------------------------------------------------ */

static int g_win_w, g_win_h;
static int g_win_ready;
static double g_last_frame;
static float g_frame_time;

/* ------------------------------------------------------------------ */
/* Window/frame                                                       */
/* ------------------------------------------------------------------ */

/* kry_instance.c owns the public InitWindow/CloseWindow/WindowShouldClose
 * and forwards to the backend seam below. */
void KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    (void)title;
    g_win_w = width;
    g_win_h = height;
    js_canvas_boot(width, height);
    g_win_ready = 1;
    g_last_frame = emscripten_get_now() / 1000.0;
}

void KryonRaylibBackend_CloseWindow(void)
{
    g_win_ready = 0;
}

bool KryonRaylibBackend_WindowShouldClose(void)
{
    return false;
}

bool IsWindowReady(void)
{
    return g_win_ready != 0;
}

bool IsWindowFocused(void)
{
    return g_win_ready != 0;
}

void SetConfigFlags(unsigned int flags)
{
    (void)flags;
}

void SetTargetFPS(int fps)
{
    (void)fps;
}

void SetWindowSize(int width, int height)
{
    g_win_w = width;
    g_win_h = height;
    js_canvas_resize(width, height);
}

void BeginDrawing(void)
{
    js_ctx_call(13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* kry_screenshot.c owns the public EndDrawing and calls this seam. */
void KryonRaylibBackend_EndDrawing(void)
{
    double now = emscripten_get_now() / 1000.0;

    EM_ASM({ if (globalThis.__kryCanvas) globalThis.__kryCanvas.frames++; });

    g_frame_time = (float)(now - g_last_frame);
    if(g_frame_time <= 0.0f)
        g_frame_time = 1.0f / 60.0f;
    g_last_frame = now;
    /* clear one-frame input edges */
    (void)js_input_query(13, 0);
    (void)js_input_query(14, 0);
    (void)js_input_query(15, 0);
    (void)js_input_query(16, 0);
    (void)js_input_query(17, 0);
    (void)js_input_query(18, 0);
    /* yield so the browser presents and pumps events */
    emscripten_sleep(1);
}

double GetTime(void)
{
    return emscripten_get_now() / 1000.0;
}

float GetFrameTime(void)
{
    return g_frame_time;
}

int GetScreenWidth(void)
{
    return g_win_w;
}

int GetScreenHeight(void)
{
    return g_win_h;
}

int GetRenderWidth(void)
{
    return g_win_w;
}

int GetRenderHeight(void)
{
    return g_win_h;
}

Vector2 GetWindowScaleDPI(void)
{
    return (Vector2){1.0f, 1.0f};
}

void SetMouseCursor(int cursor)
{
    (void)cursor;
}

void SetTraceLogLevel(int logLevel)
{
    (void)logLevel;
}

void WaitTime(double seconds)
{
    if(seconds > 0.0)
        emscripten_sleep((unsigned int)(seconds * 1000.0));
}

const char *GetApplicationDirectory(void)
{
    return "";
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
