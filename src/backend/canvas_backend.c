/*
 * canvas_backend.c — Tier A HTML5 Canvas2D backend, no raylib.
 *
 * Implements the kryon surface (the ~111 required symbols) against a 2d
 * canvas context through EM_JS: rects/circles/lines/rings/triangles as
 * canvas paths, textures as offscreen canvases (putImageData is
 * synchronous — no async ImageBitmap on the draw path), text as glyph
 * atlases rasterized from FontFace data (EM_ASYNC_JS awaits face loading;
 * the build uses -sASYNCIFY), and input through the KryonBackendRaw_*
 * hooks fed by JS event listeners.
 *
 * Only meaningful under emcc; selecting KRYON_BACKEND=canvas for a native
 * build is an error by design.
 */

#ifdef __EMSCRIPTEN__

#include "kryon_compat.generated.h"
#include "kry_input.h"
#include "kry_sw_png.h"

#include <emscripten.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* JS side: canvas, texture registry, event queues, draw helpers      */
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
        wheel: 0
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

/* Gradients need both full colors; rounded rects need a radius. */
EM_JS(void, js_draw_gradient_v, (double x, double y, double w, double h,
                                 int tr, int tg, int tb, int ta,
                                 int br, int bg, int bb, int ba), {
    var K = globalThis.__kryCanvas;
    var ctx = K.target.length ? K.target[K.target.length - 1].ctx : K.ctx;
    if (!ctx) return;
    var gr = ctx.createLinearGradient(0, y, 0, y + h);
    gr.addColorStop(0, 'rgba(' + tr + ',' + tg + ',' + tb + ',' +
                       (ta / 255.0) + ')');
    gr.addColorStop(1, 'rgba(' + br + ',' + bg + ',' + bb + ',' +
                       (ba / 255.0) + ')');
    ctx.fillStyle = gr;
    ctx.fillRect(x, y, w, h);
});

EM_JS(void, js_draw_rounded, (int op, double x, double y, double w,
                              double h, double rad,
                              int r, int gg, int bb, int aa), {
    var K = globalThis.__kryCanvas;
    var ctx = K.target.length ? K.target[K.target.length - 1].ctx : K.ctx;
    if (!ctx) return;
    var col = 'rgba(' + r + ',' + gg + ',' + bb + ',' + (aa / 255.0) + ')';
    ctx.beginPath();
    {
        var rr = Math.min(rad, Math.min(w, h) * 0.5);
        if (ctx.roundRect) {
            ctx.roundRect(x, y, w, h, rr);
        } else {
            ctx.moveTo(x + rr, y);
            ctx.lineTo(x + w - rr, y);
            ctx.arcTo(x + w, y, x + w, y + rr, rr);
            ctx.lineTo(x + w, y + h - rr);
            ctx.arcTo(x + w, y + h, x + w - rr, y + h, rr);
            ctx.lineTo(x + rr, y + h);
            ctx.arcTo(x, y + h, x, y + h - rr, rr);
            ctx.lineTo(x, y + rr);
            ctx.arcTo(x, y, x + rr, y, rr);
            ctx.closePath();
        }
    }
    if (op === 0) { ctx.fillStyle = col; ctx.fill(); }
    else { ctx.strokeStyle = col; ctx.lineWidth = 1; ctx.stroke(); }
});

/* Ring as a true filled annulus segment: outer arc forward, inner arc
 * back, even-odd fill. */
EM_JS(void, js_draw_ring, (double cx, double cy, double inner, double outer,
                           double start, double end,
                           int r, int gg, int bb, int aa), {
    var K = globalThis.__kryCanvas;
    var ctx = K.target.length ? K.target[K.target.length - 1].ctx : K.ctx;
    if (!ctx) return;
    var s0 = start * Math.PI / 180.0;
    var s1 = end * Math.PI / 180.0;
    ctx.fillStyle = 'rgba(' + r + ',' + gg + ',' + bb + ',' + (aa / 255.0) + ')';
    ctx.beginPath();
    ctx.arc(cx, cy, outer, s0, s1, false);
    ctx.arc(cx, cy, inner, s1, s0, true);
    ctx.closePath();
    ctx.fill('evenodd');
});

EM_JS(void, js_draw_texture_pro, (int id, double sx, double sy, double sw,
                                  double sh, double dx, double dy,
                                  double dw, double dh, double ox, double oy,
                                  double rot, int r, int gg, int bb, int aa),
{
    var K = globalThis.__kryCanvas;
    var ctx = K.target.length ? K.target[K.target.length - 1].ctx : K.ctx;
    var tex = K.textures[id];
    if (!ctx || !tex) return;
    var white = (r === 255 && gg === 255 && bb === 255 && aa === 255);
    if (!white) {
        /* tinted copies are cached per (texture, tint): multiply the RGB
         * channels, then restore the source alpha with destination-in */
        var key = id + ':' + r + ',' + gg + ',' + bb + ',' + aa;
        if (!K.tints) K.tints = {};
        if (!K.tints[key]) {
            var cv;
            if (globalThis.OffscreenCanvas)
                cv = new OffscreenCanvas(tex.width, tex.height);
            else {
                cv = document.createElement('canvas');
                cv.width = tex.width; cv.height = tex.height;
            }
            var c2 = cv.getContext('2d');
            c2.drawImage(tex, 0, 0);
            c2.globalCompositeOperation = 'multiply';
            c2.fillStyle = 'rgb(' + r + ',' + gg + ',' + bb + ')';
            c2.fillRect(0, 0, cv.width, cv.height);
            c2.globalCompositeOperation = 'destination-in';
            c2.drawImage(tex, 0, 0);
            K.tints[key] = cv;
        }
        tex = K.tints[key];
    }
    ctx.save();
    if (aa < 255) ctx.globalAlpha = aa / 255.0;
    ctx.translate(dx + ox, dy + oy);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    ctx.drawImage(tex, sx, sy, sw, sh, -ox, -oy, dw, dh);
    ctx.restore();
});

EM_JS(int, js_texture_from_rgba, (int ptr, int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv;
    if (globalThis.OffscreenCanvas) cv = new OffscreenCanvas(w, h);
    else if (typeof document !== 'undefined') {
        cv = document.createElement('canvas'); cv.width = w; cv.height = h;
    } else return 0;
    var c2 = cv.getContext('2d');
    var img = c2.createImageData(w, h);
    img.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
    c2.putImageData(img, 0, 0);
    var id = K.nextTex++;
    K.textures[id] = cv;
    return id;
});

EM_JS(void, js_texture_free, (int id), {
    delete globalThis.__kryCanvas.textures[id];
});

/* Create an offscreen canvas, register it as a texture, do NOT push it as
 * the draw target — BeginTextureMode selects it. */
EM_JS(int, js_render_target, (int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv;
    if (globalThis.OffscreenCanvas) cv = new OffscreenCanvas(w, h);
    else if (typeof document !== 'undefined') {
        cv = document.createElement('canvas'); cv.width = w; cv.height = h;
    } else return 0;
    var id = K.nextTex++;
    K.textures[id] = cv;
    return id;
});

EM_JS(void, js_target_select, (int id), {
    var K = globalThis.__kryCanvas;
    var cv = K.textures[id];
    if (cv) K.target.push({canvas: cv, ctx: cv.getContext('2d')});
});

EM_JS(void, js_target_deselect, (void), {
    globalThis.__kryCanvas.target.pop();
});

/* Read a texture (or the main canvas for id 0) into C memory, RGBA. */
EM_JS(int, js_texture_read, (int id, int ptr), {
    var K = globalThis.__kryCanvas;
    var cv = id === 0 ? K.canvas : K.textures[id];
    if (!cv || !cv.getContext) return 0;
    var d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data;
    HEAPU8.set(d, ptr);
    return 1;
});

/* ------------------------------------------------------------------ */
/* Text: FontFace loading (async via Asyncify) + glyph rasterization  */
/* ------------------------------------------------------------------ */

EM_ASYNC_JS(int, js_font_face_load, (int ptr, int len), {
    var g = globalThis;
    if (typeof FontFace === 'undefined') return 0;
    try {
        var buf = new Uint8Array(HEAPU8.subarray(ptr, ptr + len));
        var face = new FontFace('kry-face-' + (g.__kryFaceCount || 0), buf);
        await face.load();
        if (g.document && g.document.fonts) g.document.fonts.add(face);
        g.__kryFaceCount = (g.__kryFaceCount || 0) + 1;
        return g.__kryFaceCount;      /* 1-based face id */
    } catch (e) {
        return 0;
    }
});

EM_JS(int, js_glyph_metrics, (int face_id, int cp, int size,
                              int *adv, int *w, int *h,
                              int *offx, int *offy, int ptr),
{
    var g = globalThis;
    if (!g.__kryGlyphCv) {
        if (typeof document !== 'undefined')
            g.__kryGlyphCv = document.createElement('canvas');
        else if (g.OffscreenCanvas)
            g.__kryGlyphCv = new OffscreenCanvas(64, 64);
        else return 0;
    }
    var cv = g.__kryGlyphCv;
    var ctx = cv.getContext('2d', {willReadFrequently: true});
    var spec = size + 'px ' + (face_id > 0
        ? 'kry-face-' + (face_id - 1) : 'monospace');
    ctx.font = spec;
    ctx.textBaseline = 'alphabetic';
    var ch = String.fromCodePoint(cp);
    var m = ctx.measureText(ch);
    var advX = Math.ceil(m.width);
    var asc = Math.ceil(m.actualBoundingBoxAscent !== undefined
                        ? m.actualBoundingBoxAscent : size * 0.8);
    var desc = Math.ceil(m.actualBoundingBoxDescent !== undefined
                         ? m.actualBoundingBoxDescent : 2);
    var left = Math.ceil(m.actualBoundingBoxLeft !== undefined
                         ? m.actualBoundingBoxLeft : 0);
    var right = Math.ceil(m.actualBoundingBoxRight !== undefined
                          ? m.actualBoundingBoxRight : m.width);
    var gw = Math.max(right + left + 2, 1);
    var gh = Math.max(asc + desc + 2, 1);
    if (gw > 256 || gh > 256) return 0;
    if (cv.width < gw || cv.height < gh) {
        cv.width = Math.max(cv.width, gw);
        cv.height = Math.max(cv.height, gh);
        ctx = cv.getContext('2d', {willReadFrequently: true});
        ctx.font = spec;
        ctx.textBaseline = 'alphabetic';
    }
    ctx.clearRect(0, 0, gw, gh);
    ctx.fillStyle = '#fff';
    /* baseline sits asc+1 px into the cell; left bearing at left+1 */
    ctx.fillText(ch, left + 1, asc + 1);
    var d;
    try {
        d = ctx.getImageData(0, 0, gw, gh).data;
    } catch (e) {
        return 0;
    }
    HEAPU8.set(d.subarray(0, gw * gh * 4), ptr);
    setValue(adv, advX, 'i32');
    setValue(w, gw, 'i32');
    setValue(h, gh, 'i32');
    setValue(offx, -(left + 1), 'i32');
    setValue(offy, -(asc + 1), 'i32');
    return 1;
});

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

/* ------------------------------------------------------------------ */
/* C state                                                            */
/* ------------------------------------------------------------------ */

static int g_win_w, g_win_h;
static int g_win_ready;
static double g_last_frame;
static float g_frame_time;
static int g_default_font_built;
static Font g_default_font;

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

/* ------------------------------------------------------------------ */
/* Drawing                                                            */
/* ------------------------------------------------------------------ */

void ClearBackground(Color color)
{
    js_ctx_call(0, 0, 0, 0, 0, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    js_ctx_call(1, posX, posY, width, height, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangleRec(Rectangle rec, Color color)
{
    js_ctx_call(1, rec.x, rec.y, rec.width, rec.height, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangleLines(int posX, int posY, int width, int height,
                        Color color)
{
    js_ctx_call(2, posX, posY, width, height, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    js_ctx_call(6, rec.x, rec.y, rec.x + rec.width, rec.y + rec.height,
                lineThick, 0, 0, color.r, color.g, color.b, color.a);
}

void DrawRectangleGradientV(int posX, int posY, int width, int height,
                            Color top, Color bottom)
{
    js_draw_gradient_v(posX, posY, width, height,
                       top.r, top.g, top.b, top.a,
                       bottom.r, bottom.g, bottom.b, bottom.a);
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                          Color color)
{
    (void)segments;
    js_draw_rounded(0, rec.x, rec.y, rec.width, rec.height,
                    roundness * 0.5f * (rec.width < rec.height ? rec.width
                                                               : rec.height),
                    color.r, color.g, color.b, color.a);
}

void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                               Color color)
{
    (void)segments;
    js_draw_rounded(1, rec.x, rec.y, rec.width, rec.height,
                    roundness * 0.5f * (rec.width < rec.height ? rec.width
                                                               : rec.height),
                    color.r, color.g, color.b, color.a);
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness,
                                 int segments, float lineThick, Color color)
{
    (void)segments;
    js_draw_rounded(1, rec.x - lineThick, rec.y - lineThick,
                    rec.width + lineThick * 2, rec.height + lineThick * 2,
                    roundness * 0.5f *
                        (rec.width < rec.height ? rec.width : rec.height),
                    color.r, color.g, color.b, color.a);
}

void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    js_ctx_call(3, centerX, centerY, radius, 0, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawCircleV(Vector2 center, float radius, Color color)
{
    js_ctx_call(3, center.x, center.y, radius, 0, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    js_ctx_call(4, centerX, centerY, radius, 0, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
              Color color)
{
    js_ctx_call(5, startPosX, startPosY, endPosX, endPosY, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color)
{
    js_ctx_call(6, start.x, start.y, end.x, end.y, thick, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRing(Vector2 center, float innerRadius, float outerRadius,
              float startAngle, float endAngle, int segments, Color color)
{
    (void)segments;
    js_draw_ring(center.x, center.y, innerRadius, outerRadius,
                 startAngle, endAngle,
                 color.r, color.g, color.b, color.a);
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    js_ctx_call(7, v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, 0,
                color.r, color.g, color.b, color.a);
}

void BeginScissorMode(int x, int y, int width, int height)
{
    js_ctx_call(9, x, y, width, height, 0, 0, 0, 0, 0, 0, 0);
}

void EndScissorMode(void)
{
    js_ctx_call(10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void BeginMode2D(Camera2D camera)
{
    js_ctx_call(11, camera.zoom, camera.zoom, camera.offset.x,
                camera.offset.y, camera.target.x, camera.target.y,
                camera.rotation, 0, 0, 0, 0);
}

void EndMode2D(void)
{
    js_ctx_call(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Images & textures                                                  */
/* ------------------------------------------------------------------ */

static Image canvas_image_from_rgba(unsigned char *rgba, int w, int h)
{
    Image img;

    memset(&img, 0, sizeof(img));
    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = 1; /* PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 */
    return img;
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    int w = 0;
    int h = 0;
    unsigned char *rgba;

    (void)fileType;
    if(fileData == NULL || dataSize <= 0)
        return canvas_image_from_rgba(NULL, 0, 0);
    rgba = kry_sw_png_rgba(fileData, (size_t)dataSize, &w, &h);
    return canvas_image_from_rgba(rgba, w, h);
}

Image LoadImage(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Image img;

    if(data == NULL)
        return canvas_image_from_rgba(NULL, 0, 0);
    img = LoadImageFromMemory(".png", data, len);
    free(data);
    return img;
}

void UnloadImage(Image image)
{
    free(image.data);
}

Image LoadImageFromTexture(Texture2D texture)
{
    unsigned char *px;

    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0)
        return canvas_image_from_rgba(NULL, 0, 0);
    px = malloc((size_t)texture.width * texture.height * 4);
    if(px == NULL)
        return canvas_image_from_rgba(NULL, 0, 0);
    if(js_texture_read((int)texture.id, (int)(size_t)px) == 0) {
        free(px);
        return canvas_image_from_rgba(NULL, 0, 0);
    }
    return canvas_image_from_rgba(px, texture.width, texture.height);
}

void ImageFlipVertical(Image *image)
{
    int w, h, y;
    unsigned char *row;

    if(image == NULL || image->data == NULL)
        return;
    w = image->width;
    h = image->height;
    row = malloc((size_t)w * 4);
    if(row == NULL)
        return;
    for(y = 0; y < h / 2; y++) {
        unsigned char *top = (unsigned char *)image->data + (size_t)y * w * 4;
        unsigned char *bot = (unsigned char *)image->data +
            (size_t)(h - 1 - y) * w * 4;

        memcpy(row, top, (size_t)w * 4);
        memcpy(top, bot, (size_t)w * 4);
        memcpy(bot, row, (size_t)w * 4);
    }
    free(row);
}

Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex;

    memset(&tex, 0, sizeof(tex));
    if(image.data == NULL || image.width <= 0 || image.height <= 0)
        return tex;
    tex.id = (unsigned)js_texture_from_rgba((int)(size_t)image.data,
                                            image.width, image.height);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = 1;
    return tex;
}

Texture2D LoadTexture(const char *fileName)
{
    Image img = LoadImage(fileName);
    Texture2D tex = LoadTextureFromImage(img);

    UnloadImage(img);
    return tex;
}

void UnloadTexture(Texture2D texture)
{
    if(texture.id != 0)
        js_texture_free((int)texture.id);
}

void SetTextureFilter(Texture2D texture, int filter)
{
    (void)texture;
    (void)filter;
}

RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D rt;

    memset(&rt, 0, sizeof(rt));
    rt.id = (unsigned)js_render_target(width, height);
    rt.texture.id = rt.id;
    rt.texture.width = width;
    rt.texture.height = height;
    rt.texture.mipmaps = 1;
    rt.texture.format = 1;
    return rt;
}

void UnloadRenderTexture(RenderTexture2D target)
{
    if(target.id != 0)
        js_texture_free((int)target.id);
}

void BeginTextureMode(RenderTexture2D target)
{
    js_target_select((int)target.id);
}

void EndTextureMode(void)
{
    js_target_deselect();
}

void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width,
                               (float)texture.height},
                   (Rectangle){(float)posX, (float)posY,
                               (float)texture.width, (float)texture.height},
                   (Vector2){0, 0}, 0.0f, tint);
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    js_draw_texture_pro((int)texture.id, source.x, source.y,
                        source.width, source.height, dest.x, dest.y,
                        dest.width, dest.height, origin.x, origin.y,
                        rotation, tint.r, tint.g, tint.b, tint.a);
}

/* ------------------------------------------------------------------ */
/* Text: atlas building                                               */
/* ------------------------------------------------------------------ */

#define CANVAS_ATLAS_START_W 512
#define CANVAS_ATLAS_START_H 64

static int canvas_font_build(Font *out, int face_id, int baseSize,
                             const int *codepoints, int count)
{
    unsigned char *pixels;
    int atlas_w = CANVAS_ATLAS_START_W;
    int atlas_h = CANVAS_ATLAS_START_H;
    int x = 1;
    int y = 1;
    int row_h = 0;
    int i;
    int ok = 0;

    if(out == NULL || codepoints == NULL || count <= 0 || baseSize <= 0)
        return 0;
    memset(out, 0, sizeof(*out));
    out->glyphs = calloc((size_t)count, sizeof(GlyphInfo));
    out->recs = calloc((size_t)count, sizeof(Rectangle));
    pixels = malloc((size_t)atlas_w * atlas_h * 4);
    if(out->glyphs == NULL || out->recs == NULL || pixels == NULL)
        goto fail;
    memset(pixels, 0, (size_t)atlas_w * atlas_h * 4);
    for(i = 0; i < count; i++) {
        int adv = 0, gw = 0, gh = 0, offx = 0, offy = 0;
        unsigned char gbuf[256 * 256 * 4];

        if(codepoints[i] < 32)
            continue;
        if(js_glyph_metrics(face_id, codepoints[i], baseSize,
                            &adv, &gw, &gh, &offx, &offy,
                            (int)(size_t)gbuf) == 0)
            continue;
        if(x + gw + 1 > atlas_w) {
            x = 1;
            y += row_h + 1;
            row_h = 0;
        }
        while(y + gh + 1 > atlas_h && atlas_h < 4096)
            atlas_h *= 2;
        if(y + gh + 1 > atlas_h)
            continue;
        {
            unsigned char *grown = realloc(pixels,
                                           (size_t)atlas_w * atlas_h * 4);

            if(grown == NULL)
                continue;
            pixels = grown;
        }
        {
            int gy;

            for(gy = 0; gy < gh; gy++) {
                memcpy(pixels + (size_t)(y + gy) * atlas_w * 4 +
                       (size_t)x * 4,
                       gbuf + (size_t)gy * gw * 4, (size_t)gw * 4);
            }
        }
        out->recs[i] = (Rectangle){(float)x, (float)y, (float)gw, (float)gh};
        out->glyphs[i].value = codepoints[i];
        out->glyphs[i].offsetX = offx;
        out->glyphs[i].offsetY = offy;
        out->glyphs[i].advanceX = adv;
        x += gw + 1;
        if(gh > row_h)
            row_h = gh;
    }
    out->baseSize = baseSize;
    out->glyphCount = count;
    out->glyphPadding = 0;
    out->texture.id = (unsigned)js_texture_from_rgba((int)(size_t)pixels,
                                                     atlas_w, atlas_h);
    out->texture.width = atlas_w;
    out->texture.height = atlas_h;
    out->texture.mipmaps = 1;
    out->texture.format = 1;
    ok = out->texture.id != 0;
fail:
    free(pixels);
    if(!ok) {
        free(out->glyphs);
        free(out->recs);
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

Font GetFontDefault(void)
{
    if(!g_default_font_built) {
        static int cps[96];
        int i;

        for(i = 0; i < 95; i++)
            cps[i] = 32 + i;
        if(canvas_font_build(&g_default_font, 0, 16, cps, 95))
            g_default_font_built = 1;
    }
    return g_default_font;
}

Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize, int fontSize, const int *codepoints,
                        int glyphCount)
{
    Font font;
    int face_id;

    (void)fileType;
    memset(&font, 0, sizeof(font));
    if(fileData == NULL || dataSize <= 0 || fontSize <= 0)
        return font;
    face_id = js_font_face_load((int)(size_t)fileData, dataSize);
    if(face_id == 0)
        return font;
    if(codepoints == NULL || glyphCount <= 0) {
        static int default_set[224];
        int i;

        for(i = 0; i < 224; i++)
            default_set[i] = 32 + i;
        glyphCount = 224;
        codepoints = default_set;
    }
    if(!canvas_font_build(&font, face_id, fontSize, codepoints, glyphCount))
        memset(&font, 0, sizeof(font));
    return font;
}

GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize,
                        int fontSize, const int *codepoints,
                        int codepointCount, int type, int *glyphCount)
{
    Font font;

    (void)type;
    if(glyphCount != NULL)
        *glyphCount = 0;
    font = LoadFontFromMemory(".ttf", fileData, dataSize, fontSize,
                              codepoints, codepointCount);
    if(font.glyphs == NULL)
        return NULL;
    if(glyphCount != NULL)
        *glyphCount = font.glyphCount;
    /* caller owns the array; the transient Font's atlas is kept alive by
     * its texture id in JS (intentionally not unloaded) */
    return font.glyphs;
}

Font LoadFont(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font;

    if(data == NULL)
        return font;
    memset(&font, 0, sizeof(font));
    font = LoadFontFromMemory(".ttf", data, len, 16, NULL, 0);
    free(data);
    return font;
}

void UnloadFont(Font font)
{
    if(font.glyphs != NULL)
        free(font.glyphs);
    if(font.recs != NULL)
        free(font.recs);
    if(font.texture.id != 0)
        js_texture_free((int)font.texture.id);
}

static int canvas_glyph_index(const Font *font, int codepoint)
{
    int i;

    if(font == NULL || font->glyphs == NULL)
        return 0;
    for(i = 0; i < font->glyphCount; i++)
        if(font->glyphs[i].value == codepoint)
            return i;
    return 0;
}

int GetGlyphIndex(Font font, int codepoint)
{
    return canvas_glyph_index(&font, codepoint);
}

GlyphInfo GetGlyphInfo(Font font, int codepoint)
{
    int i = canvas_glyph_index(&font, codepoint);

    if(font.glyphs == NULL)
        return (GlyphInfo){0};
    return font.glyphs[i];
}

void DrawText(const char *text, int posX, int posY, int fontSize,
              Color color)
{
    Font font = GetFontDefault();
    float scale;
    float x;
    int i;

    if(text == NULL || font.glyphs == NULL || font.recs == NULL)
        return;
    scale = fontSize > 0 ? (float)fontSize / (float)font.baseSize : 1.0f;
    x = (float)posX;
    for(i = 0; text[i] != '\0'; ) {
        unsigned cp = 0;
        int gi;

        if((text[i] & 0x80) == 0) {
            cp = (unsigned char)text[i];
            i++;
        } else if((text[i] & 0xe0) == 0xc0 && text[i + 1] != '\0') {
            cp = ((unsigned char)text[i] & 0x1f) << 6 |
                 ((unsigned char)text[i + 1] & 0x3f);
            i += 2;
        } else if((text[i] & 0xf0) == 0xe0 && text[i + 1] != '\0' &&
                  text[i + 2] != '\0') {
            cp = ((unsigned char)text[i] & 0x0f) << 12 |
                 ((unsigned char)text[i + 1] & 0x3f) << 6 |
                 ((unsigned char)text[i + 2] & 0x3f);
            i += 3;
        } else {
            i++;
            continue;
        }
        gi = canvas_glyph_index(&font, (int)cp);
        DrawTexturePro(font.texture, font.recs[gi],
                       (Rectangle){x + font.glyphs[gi].offsetX * scale,
                                   (float)posY +
                                       font.glyphs[gi].offsetY * scale,
                                   font.recs[gi].width * scale,
                                   font.recs[gi].height * scale},
                       (Vector2){0, 0}, 0.0f, color);
        x += font.glyphs[gi].advanceX * scale;
    }
}

/* ------------------------------------------------------------------ */
/* OS services                                                        */
/* ------------------------------------------------------------------ */

/* navigator.clipboard is async, so the canvas backend keeps a mirror of
 * the last text the app wrote: copy/paste round-trips within the app,
 * and the browser write is attempted fire-and-forget. */
static char g_clipboard[4096];

const char *GetClipboardText(void)
{
    return g_clipboard;
}

void SetClipboardText(const char *text)
{
    if(text == NULL)
        return;
    snprintf(g_clipboard, sizeof(g_clipboard), "%s", text);
    EM_ASM({
        if (globalThis.navigator && globalThis.navigator.clipboard &&
            globalThis.navigator.clipboard.writeText)
            globalThis.navigator.clipboard.writeText(UTF8ToString($0));
    }, text);
}

bool FileExists(const char *fileName)
{
    struct stat st;

    return fileName != NULL && stat(fileName, &st) == 0 &&
           !S_ISDIR(st.st_mode);
}

bool DirectoryExists(const char *dirPath)
{
    struct stat st;

    return dirPath != NULL && stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode);
}

const char *GetDirectoryPath(const char *filePath)
{
    static char dir[1024];
    const char *slash;

    if(filePath == NULL)
        return ".";
    slash = strrchr(filePath, '/');
    if(slash == NULL || slash == filePath)
        return ".";
    snprintf(dir, sizeof(dir), "%.*s", (int)(slash - filePath), filePath);
    return dir;
}

int MakeDirectory(const char *dirPath)
{
    if(dirPath == NULL)
        return 0;
    return mkdir(dirPath, 0777) == 0 ? 1 : 0;
}

unsigned char *LoadFileData(const char *fileName, int *dataSize)
{
    FILE *f;
    long len;
    unsigned char *data;

    if(dataSize != NULL)
        *dataSize = 0;
    if(fileName == NULL)
        return NULL;
    f = fopen(fileName, "rb");
    if(f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len <= 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)len);
    if(data == NULL || fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if(dataSize != NULL)
        *dataSize = (int)len;
    return data;
}

void UnloadFileData(unsigned char *data)
{
    free(data);
}

char *LoadFileText(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    char *text;

    if(data == NULL)
        return NULL;
    text = malloc((size_t)len + 1);
    if(text != NULL) {
        memcpy(text, data, (size_t)len);
        text[len] = '\0';
    }
    free(data);
    return text;
}

void UnloadFileText(char *text)
{
    free(text);
}

void OpenURL(const char *url)
{
    if(url == NULL)
        return;
    EM_ASM({
        if (typeof window !== 'undefined')
            window.open(UTF8ToString($0), '_blank');
    }, url);
}

/* ------------------------------------------------------------------ */
/* Misc surface                                                       */
/* ------------------------------------------------------------------ */

const char *TextFormat(const char *text, ...)
{
    static char buffers[16][256];
    static int next;
    va_list args;

    va_start(args, text);
    vsnprintf(buffers[next], sizeof(buffers[0]), text, args);
    va_end(args);
    return buffers[next++ % 16];
}

void TraceLog(int logLevel, const char *text, ...)
{
    va_list args;

    (void)logLevel;
    va_start(args, text);
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
    va_end(args);
}

/* Image generation: one plain color (the surface subset apps use). */
Image GenImageColor(int width, int height, Color color)
{
    Image img;
    unsigned char *px;
    int i;

    memset(&img, 0, sizeof(img));
    if(width <= 0 || height <= 0)
        return img;
    px = malloc((size_t)width * height * 4);
    if(px == NULL)
        return img;
    for(i = 0; i < width * height; i++) {
        px[i * 4 + 0] = color.r;
        px[i * 4 + 1] = color.g;
        px[i * 4 + 2] = color.b;
        px[i * 4 + 3] = color.a;
    }
    img.data = px;
    img.width = width;
    img.height = height;
    img.mipmaps = 1;
    img.format = 1;
    return img;
}

void SetShapesTexture(Texture2D texture, Rectangle rec)
{
    (void)texture;
    (void)rec;
}

void SetTraceLogCallback(TraceLogCallback callback)
{
    (void)callback;
}

/* ExportImage: reuse kry_screenshot.c's internal PNG writer. */
extern int kry_write_png_file(const char *path, const unsigned char *rgba,
                              int w, int h);

bool ExportImage(Image image, const char *fileName)
{
    if(image.data == NULL || fileName == NULL)
        return false;
    return kry_write_png_file(fileName, (const unsigned char *)image.data,
                              image.width, image.height) == 0;
}

/* Audio: null-grade stubs (UI-only backend). */
Sound LoadSound(const char *fileName)
{
    (void)fileName;
    return (Sound){0};
}
Sound LoadSoundFromWave(Wave wave)
{
    (void)wave;
    return (Sound){0};
}
void UnloadSound(Sound sound)
{
    (void)sound;
}
void PlaySound(Sound sound)
{
    (void)sound;
}
void StopSound(Sound sound)
{
    (void)sound;
}
void SetSoundVolume(Sound sound, float volume)
{
    (void)sound;
    (void)volume;
}
void SetSoundPitch(Sound sound, float pitch)
{
    (void)sound;
    (void)pitch;
}
Music LoadMusicStream(const char *fileName)
{
    (void)fileName;
    return (Music){0};
}
void UnloadMusicStream(Music music)
{
    (void)music;
}
void PlayMusicStream(Music music)
{
    (void)music;
}
void StopMusicStream(Music music)
{
    (void)music;
}
void UpdateMusicStream(Music music)
{
    (void)music;
}
void SetMusicVolume(Music music, float volume)
{
    (void)music;
    (void)volume;
}

/* Audio device/wave surface (null-grade): apps drive these directly. */
void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}
bool IsAudioDeviceReady(void)
{
    return false;
}
Wave LoadWave(const char *fileName)
{
    (void)fileName;
    return (Wave){0};
}
Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize)
{
    (void)fileType;
    (void)fileData;
    (void)dataSize;
    return (Wave){0};
}
void UnloadWave(Wave wave)
{
    (void)wave;
}
void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels)
{
    (void)wave;
    (void)sampleRate;
    (void)sampleSize;
    (void)channels;
}
bool IsSoundPlaying(Sound sound)
{
    (void)sound;
    return false;
}
void AttachAudioMixedProcessor(void (*processor)(void *buffer,
                                                 unsigned int frames))
{
    (void)processor;
}
void DetachAudioMixedProcessor(void (*processor)(void *buffer,
                                                 unsigned int frames))
{
    (void)processor;
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
