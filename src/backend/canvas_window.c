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

EM_JS(void, js_canvas_boot, (int w, int h, const char *title), {
    var g = globalThis;
    var K = g.__kryCanvas = {
        w: w, h: h, renderW: w, renderH: h, dpi: 1,
        canvas: null, ctx: null,
        textures: {}, nextTex: 1,
        target: [],               /* render-target stack: {canvas,ctx} */
        saved: 0,                 /* outstanding ctx.save() pairs */
        keysDown: {}, keysPressed: [], keysReleased: [], keysRepeated: [],
        chars: [],
        mouseX: 0, mouseY: 0, mouseDeltaX: 0, mouseDeltaY: 0,
        mouseOffsetX: 0, mouseOffsetY: 0, mouseScaleX: 1, mouseScaleY: 1,
        buttonsDown: {}, buttonsPressed: [], buttonsReleased: [],
        wheelX: 0, wheelY: 0,
        touches: [], dropped: [], droppedPending: 0, focused: 1, resized: 0,
        clipboard: "", clipboardGestureUntil: 0,
        gamepadPrev: [], gamepadNow: [], gamepadPressed: [],
        gamepadReleased: [], lastGamepadButton: -1,
        frames: 0, lastOp: 'boot'
    };
    K.ctxNow = function () {
        return K.target.length ? K.target[K.target.length - 1].ctx
                               : K.ctx;
    };
    K.col = function (r, g, b, a) {
        return 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255.0) + ')';
    };
    K.makeCanvas = function (w, h) {
        if (globalThis.OffscreenCanvas) return new OffscreenCanvas(w, h);
        if (typeof document !== 'undefined') {
            var cv = document.createElement('canvas');
            cv.width = w; cv.height = h;
            return cv;
        }
        return null;
    };
    K.resizeMainCanvas = function (nw, nh) {
        var dpr = Math.max(1, g.devicePixelRatio || 1);
        var c = K.canvas;
        /* Shells may lay the canvas out inside a card instead of filling
         * the viewport. The element's CSS layout box is the real drawable:
         * a backing store sized to the window would be CSS-stretched into
         * that box and distort the render. Ask for nw/nh, then let the
         * laid-out box win when CSS sizes the element. */
        if (c && c.style) {
            c.style.width = Math.max(1, nw | 0) + 'px';
            c.style.height = Math.max(1, nh | 0) + 'px';
            c.style.touchAction = 'none';
        }
        if (c && c.clientWidth > 0 && c.clientHeight > 0) {
            nw = c.clientWidth;
            nh = c.clientHeight;
        }
        nw = Math.max(1, nw | 0);
        nh = Math.max(1, nh | 0);
        if (K.ctx && nw === K.w && nh === K.h &&
            c && c.width === K.renderW && c.height === K.renderH)
            return;
        K.w = nw;
        K.h = nh;
        K.dpi = dpr;
        K.renderW = Math.max(1, Math.round(K.w * dpr));
        K.renderH = Math.max(1, Math.round(K.h * dpr));
        if (c) {
            /* Assigning the width/height attributes clears the bitmap, so
             * only touch them when the size actually changed. */
            if (c.width !== K.renderW || c.height !== K.renderH) {
                K.resized = 1;
                c.width = K.renderW;
                c.height = K.renderH;
            }
            K.ctx = c.getContext('2d');
        }
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
    if (K.canvas) K.resizeMainCanvas(w, h);
    if (doc && doc.title !== undefined) doc.title = title ? UTF8ToString(title) : "";
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
    var keyOf = function (code, key) {
        if (!code && key) {
            if (key.length === 1) {
                var kc = key.toUpperCase().charCodeAt(0);
                if (kc >= 32 && kc <= 126) return kc;
            }
            code = key;
        }
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
    var localPoint = function (e) {
        var r = K.canvas && K.canvas.getBoundingClientRect
              ? K.canvas.getBoundingClientRect() : {left: 0, top: 0};
        return {x: (e.clientX - r.left - K.mouseOffsetX) * K.mouseScaleX,
                y: (e.clientY - r.top - K.mouseOffsetY) * K.mouseScaleY};
    };
    var setMouse = function (x, y) {
        K.mouseDeltaX += x - K.mouseX;
        K.mouseDeltaY += y - K.mouseY;
        K.mouseX = x; K.mouseY = y;
    };
    var syncTouches = function (list) {
        var hadTouches = K.touches.length > 0;
        K.touches = [];
        for (var i = 0; i < list.length && i < 8; i++) {
            var t = list[i];
            var p = localPoint(t);
            K.touches.push({id: t.identifier | 0, x: p.x, y: p.y});
        }
        if (K.touches.length > 0) setMouse(K.touches[0].x, K.touches[0].y);
        if (!hadTouches && K.touches.length > 0) {
            K.buttonsDown[0] = 1;
            K.buttonsPressed.push(0);
        } else if (hadTouches && K.touches.length === 0) {
            delete K.buttonsDown[0];
            K.buttonsReleased.push(0);
        }
    };
    var hook = function (target) {
        if (!target || target.__kryHooked || !target.addEventListener)
            return;
        target.__kryHooked = 1;
        target.addEventListener('mousemove', function (e) {
            var p = localPoint(e);
            setMouse(p.x, p.y);
        });
        target.addEventListener('mousedown', function (e) {
            var p = localPoint(e);
            setMouse(p.x, p.y);
            if (K.canvas && K.canvas.focus) {
                try { K.canvas.focus({preventScroll: true}); } catch (_) {
                    try { K.canvas.focus(); } catch (_) {}
                }
            }
            K.buttonsDown[e.button] = 1;
            K.buttonsPressed.push(e.button);
            if (K.canvas && K.canvas.setPointerCapture &&
                e.pointerId !== undefined) {
                try { K.canvas.setPointerCapture(e.pointerId); } catch (_) {}
            }
        });
        target.addEventListener('mouseup', function (e) {
            var p = localPoint(e);
            setMouse(p.x, p.y);
            delete K.buttonsDown[e.button];
            K.buttonsReleased.push(e.button);
        });
        target.addEventListener('wheel', function (e) {
            var unit = e.deltaMode === 1 ? 16 : (e.deltaMode === 2 ? K.h : 120);
            K.wheelX += -e.deltaX / unit;
            K.wheelY += -e.deltaY / unit;
            if (e.cancelable) e.preventDefault();
        });
        target.addEventListener('keydown', function (e) {
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                if (K.keysDown[k] && e.repeat) K.keysRepeated.push(k);
                else if (!K.keysDown[k]) K.keysPressed.push(k);
                K.keysDown[k] = 1;
                if ((e.ctrlKey || e.metaKey) &&
                    (k === 65 || k === 67 || k === 86 || k === 88)) {
                    var nowMs = (typeof performance !== 'undefined' &&
                        performance.now) ? performance.now() : Date.now();
                    K.clipboardGestureUntil = nowMs + 1000;
                    if (e.cancelable) e.preventDefault();
                }
                if (k === 32 || (k >= 256 && k <= 269))
                    e.preventDefault();
            }
        });
        target.addEventListener('keyup', function (e) {
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                delete K.keysDown[k];
                K.keysReleased.push(k);
                if (k === 32 || (k >= 256 && k <= 269))
                    e.preventDefault();
            }
        });
        target.addEventListener('keypress', function (e) {
            if (e.key && e.key.length === 1)
                K.chars.push(e.key.charCodeAt(0));
        });
        target.addEventListener('touchstart', function (e) {
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchmove', function (e) {
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchend', function (e) {
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchcancel', function (e) {
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
    };
    var hookDrop = function (target) {
        if (!target || target.__kryDropHooked || !target.addEventListener)
            return;
        target.__kryDropHooked = 1;
        target.addEventListener('dragover', function (e) {
            if (e.preventDefault) e.preventDefault();
        });
        target.addEventListener('drop', function (e) {
            if (e.preventDefault) e.preventDefault();
            var files = e.dataTransfer && e.dataTransfer.files
                      ? e.dataTransfer.files : [];
            K.dropped = [];
            K.droppedPending = files.length | 0;
            Array.prototype.forEach.call(files, function (file) {
                var name = (file.name || 'dropped-file')
                    .replace(new RegExp('[\\\\/]', 'g'), '_');
                var path = '/dropped/' + name;
                if (typeof FS !== 'undefined' && FS.mkdir) {
                    try { FS.mkdir('/dropped'); } catch (_) {}
                }
                if (file.arrayBuffer) {
                    file.arrayBuffer().then(function (buf) {
                        if (typeof FS !== 'undefined' && FS.writeFile)
                            FS.writeFile(path, new Uint8Array(buf));
                        K.dropped.push(path);
                    }).finally(function () {
                        K.droppedPending = Math.max(0, K.droppedPending - 1);
                    });
                } else {
                    K.droppedPending = Math.max(0, K.droppedPending - 1);
                }
            });
        });
    };
    if (doc) hook(doc);
    hook(g);
    hookDrop(K.canvas);
    hookDrop(doc);
    if (g.addEventListener) {
        g.addEventListener('focus', function () { K.focused = 1; });
        g.addEventListener('blur', function () { K.focused = 0; });
        g.addEventListener('paste', function (e) {
            var data = e.clipboardData || (g.clipboardData || null);
            var text = data && data.getData ? data.getData('text') : "";
            if (text) K.clipboard = text;
        });
        g.addEventListener('copy', function (e) {
            if (!K.clipboard) return;
            var data = e.clipboardData || (g.clipboardData || null);
            if (data && data.setData) {
                data.setData('text/plain', K.clipboard);
                if (e.preventDefault) e.preventDefault();
            }
        });
        g.addEventListener('cut', function (e) {
            if (!K.clipboard) return;
            var data = e.clipboardData || (g.clipboardData || null);
            if (data && data.setData) {
                data.setData('text/plain', K.clipboard);
                if (e.preventDefault) e.preventDefault();
            }
        });
    }
    if (K.canvas && g.ResizeObserver) {
        K.ro = new ResizeObserver(function (entries) {
            var cr = entries && entries[0] && entries[0].contentRect;
            if (cr && cr.width > 0 && cr.height > 0 &&
                (Math.round(cr.width) !== K.w || Math.round(cr.height) !== K.h))
                K.resizeMainCanvas(Math.round(cr.width), Math.round(cr.height));
        });
        try { K.ro.observe(K.canvas); } catch (_) {}
    }
});

EM_JS(void, js_canvas_resize, (int w, int h), {
    var K = globalThis.__kryCanvas;
    if (K && K.resizeMainCanvas) K.resizeMainCanvas(w, h);
});

EM_JS(int, js_canvas_dim, (int which), {
    var K = globalThis.__kryCanvas;
    if (!K) return 0;
    switch (which) {
    case 0: return K.w | 0;
    case 1: return K.h | 0;
    case 2: return K.renderW | 0;
    case 3: return K.renderH | 0;
    case 4: { var r = K.resized ? 1 : 0; K.resized = 0; return r; }
    case 5: return K.focused ? 1 : 0;
    }
    return 0;
});

EM_JS(double, js_canvas_dpi, (void), {
    var K = globalThis.__kryCanvas;
    return K ? K.dpi : 1.0;
});

EM_JS(void, js_canvas_set_cursor, (int cursor), {
    var K = globalThis.__kryCanvas;
    if (!K) return;
    var canvas = K && K.canvas;
    var value = 'default';
    switch (cursor) {
    case 0: value = 'default'; break;
    case 1: value = 'default'; break;
    case 2: value = 'text'; break;
    case 3: value = 'crosshair'; break;
    case 4: value = 'pointer'; break;
    case 5: value = 'ew-resize'; break;
    case 6: value = 'ns-resize'; break;
    case 7: value = 'nwse-resize'; break;
    case 8: value = 'nesw-resize'; break;
    case 9: value = 'move'; break;
    case 10: value = 'not-allowed'; break;
    default: value = 'default'; break;
    }
    K.cursor = value;
    var apply = function (node) {
        if (node && node.style) node.style.cursor = value;
    };
    apply(canvas);
    apply(canvas && canvas.parentElement);
    if (typeof document !== 'undefined') {
        apply(document.body);
        apply(document.documentElement);
    }
});

EM_JS(void, js_canvas_set_title, (const char *title), {
    if (typeof document !== 'undefined' && document.title !== undefined)
        document.title = title ? UTF8ToString(title) : "";
});

EM_JS(void, js_canvas_focus, (void), {
    var K = globalThis.__kryCanvas;
    if (K && K.canvas && K.canvas.focus) K.canvas.focus();
});

EM_JS(void, js_canvas_fullscreen, (int enable), {
    var K = globalThis.__kryCanvas;
    if (typeof document === 'undefined' || !K || !K.canvas) return;
    if (enable) {
        if (!document.fullscreenElement && K.canvas.requestFullscreen)
            K.canvas.requestFullscreen().catch(function () {});
    } else {
        if (document.fullscreenElement && document.exitFullscreen)
            document.exitFullscreen().catch(function () {});
    }
});

EM_JS(int, js_canvas_fullscreen_state, (void), {
    if (typeof document === 'undefined') return 0;
    return document.fullscreenElement ? 1 : 0;
});

EM_JS(void, js_ctx_call, (int op, double a, double b, double c, double d,
                          double e, double f, double g2,
                          int r, int gg, int bb, int aa),
{
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    K.lastOp = 'ctx' + op;
    if (!ctx) return;
    var col = K.col(r, gg, bb, aa);
    switch (op) {
    case 0: { /* clear */
            var tw = K.target.length ? K.target[K.target.length - 1].canvas.width : K.w;
            var th = K.target.length ? K.target[K.target.length - 1].canvas.height : K.h;
            ctx.fillStyle = col;
            ctx.fillRect(0, 0, tw, th); break; }
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
    case 9: /* scissor begin: raylib replaces the active scissor */
            while (K.saved > 0) { ctx.restore(); K.saved--; }
            ctx.save(); ctx.beginPath();
            ctx.rect(a, b, c, d); ctx.clip(); K.saved = 1; break;
    case 10: /* scissor end */ if (K.saved > 0) { ctx.restore(); K.saved = 0; }
             break;
    case 11: /* mode2d push: raylib camera transform */
             ctx.save();
             ctx.translate(c, d);          /* offset */
             ctx.rotate(-g2 * Math.PI / 180.0);
             ctx.scale(a, b);              /* zoom */
             ctx.translate(-e, -f);        /* -target */
             break;
    case 12: /* mode2d pop */ ctx.restore(); break;
    case 13: /* frame reset: logical screen coords over HiDPI backing */
             while (K.saved > 0) { ctx.restore(); K.saved--; }
             ctx.setTransform(K.target.length ? 1 : K.dpi, 0, 0,
                              K.target.length ? 1 : K.dpi, 0, 0);
             K.saved = 0;
             break;
    }
});

/* ------------------------------------------------------------------ */
/* C state                                                            */
/* ------------------------------------------------------------------ */

static int g_win_w, g_win_h;
static int g_win_ready;
static int g_exit_key = KEY_ESCAPE;
static unsigned int g_window_state;
static double g_last_frame;
static float g_frame_time;
static int g_target_fps;

/* ------------------------------------------------------------------ */
/* Window/frame                                                       */
/* ------------------------------------------------------------------ */

/* kry_instance.c owns the public InitWindow/CloseWindow/WindowShouldClose
 * and forwards to the backend seam below. */
void KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    g_win_w = width;
    g_win_h = height;
    js_canvas_boot(width, height, title);
    g_win_ready = 1;
    g_last_frame = emscripten_get_now() / 1000.0;
}

void KryonRaylibBackend_CloseWindow(void)
{
    g_win_ready = 0;
}

bool KryonRaylibBackend_WindowShouldClose(void)
{
    return g_exit_key != 0 && js_input_query(1, g_exit_key) != 0;
}

bool IsWindowReady(void)
{
    return g_win_ready != 0;
}

bool IsWindowFocused(void)
{
    return g_win_ready != 0 && js_canvas_dim(5) != 0;
}

bool IsWindowFullscreen(void)
{
    return ((g_window_state & FLAG_FULLSCREEN_MODE) != 0) ||
           js_canvas_fullscreen_state() != 0;
}

bool IsWindowHidden(void)
{
    return (g_window_state & FLAG_WINDOW_HIDDEN) != 0;
}

bool IsWindowMinimized(void)
{
    return (g_window_state & FLAG_WINDOW_MINIMIZED) != 0;
}

bool IsWindowMaximized(void)
{
    return (g_window_state & FLAG_WINDOW_MAXIMIZED) != 0;
}

bool IsWindowResized(void)
{
    return js_canvas_dim(4) != 0;
}

bool IsWindowState(unsigned int flag)
{
    return (g_window_state & flag) != 0;
}

void SetConfigFlags(unsigned int flags)
{
    g_window_state |= flags;
}

void SetWindowState(unsigned int flags)
{
    g_window_state |= flags;
    if((flags & FLAG_FULLSCREEN_MODE) != 0)
        js_canvas_fullscreen(1);
}

void ClearWindowState(unsigned int flags)
{
    g_window_state &= ~flags;
    if((flags & FLAG_FULLSCREEN_MODE) != 0)
        js_canvas_fullscreen(0);
}

void ToggleFullscreen(void)
{
    if(IsWindowFullscreen()) {
        g_window_state &= ~FLAG_FULLSCREEN_MODE;
        js_canvas_fullscreen(0);
    } else {
        g_window_state |= FLAG_FULLSCREEN_MODE;
        js_canvas_fullscreen(1);
    }
}

void ToggleBorderlessWindowed(void)
{
    g_window_state ^= FLAG_BORDERLESS_WINDOWED_MODE;
}

void MaximizeWindow(void)
{
    g_window_state |= FLAG_WINDOW_MAXIMIZED;
    g_window_state &= ~FLAG_WINDOW_MINIMIZED;
}

void MinimizeWindow(void)
{
    g_window_state |= FLAG_WINDOW_MINIMIZED;
    g_window_state &= ~FLAG_WINDOW_MAXIMIZED;
}

void RestoreWindow(void)
{
    g_window_state &= ~(FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED |
                        FLAG_FULLSCREEN_MODE |
                        FLAG_BORDERLESS_WINDOWED_MODE);
}

void SetTargetFPS(int fps)
{
    g_target_fps = fps > 0 ? fps : 0;
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
    double elapsed;
    unsigned int sleep_ms = 1;

    EM_ASM({ if (globalThis.__kryCanvas) globalThis.__kryCanvas.frames++; });

    elapsed = now - g_last_frame;
    g_frame_time = (float)elapsed;
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
    (void)js_input_query(20, 0);
    js_input_end_frame();
    /* yield so the browser presents and pumps events, respecting SetTargetFPS */
    if(g_target_fps > 0) {
        double target = 1.0 / (double)g_target_fps;

        if(elapsed < target) {
            double wait = (target - elapsed) * 1000.0;

            sleep_ms = wait > 1.0 ? (unsigned int)wait : 1;
        }
    }
    emscripten_sleep(sleep_ms);
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
    int w = js_canvas_dim(0);

    return w > 0 ? w : g_win_w;
}

int GetScreenHeight(void)
{
    int h = js_canvas_dim(1);

    return h > 0 ? h : g_win_h;
}

int GetRenderWidth(void)
{
    int w = js_canvas_dim(2);

    return w > 0 ? w : GetScreenWidth();
}

int GetRenderHeight(void)
{
    int h = js_canvas_dim(3);

    return h > 0 ? h : GetScreenHeight();
}

Vector2 GetWindowScaleDPI(void)
{
    float dpr = (float)js_canvas_dpi();

    return (Vector2){dpr, dpr};
}

void SetMouseCursor(int cursor)
{
    js_canvas_set_cursor(cursor);
}

void SetWindowTitle(const char *title)
{
    js_canvas_set_title(title);
}

void SetWindowFocused(void)
{
    js_canvas_focus();
}

void SetWindowIcon(Image image)
{
    (void)image;
}

void SetWindowIcons(Image *images, int count)
{
    (void)images;
    (void)count;
}

void SetWindowPosition(int x, int y)
{
    (void)x;
    (void)y;
}

void SetWindowMonitor(int monitor)
{
    (void)monitor;
}

void SetWindowMinSize(int width, int height)
{
    (void)width;
    (void)height;
}

void SetWindowMaxSize(int width, int height)
{
    (void)width;
    (void)height;
}

void SetWindowOpacity(float opacity)
{
    (void)opacity;
}

void *GetWindowHandle(void)
{
    return NULL;
}

int GetMonitorCount(void)
{
    return 1;
}

int GetCurrentMonitor(void)
{
    return 0;
}

Vector2 GetMonitorPosition(int monitor)
{
    (void)monitor;
    return (Vector2){0.0f, 0.0f};
}

int GetMonitorWidth(int monitor)
{
    (void)monitor;
    return GetScreenWidth();
}

int GetMonitorHeight(int monitor)
{
    (void)monitor;
    return GetScreenHeight();
}

int GetMonitorPhysicalWidth(int monitor)
{
    (void)monitor;
    return 0;
}

int GetMonitorPhysicalHeight(int monitor)
{
    (void)monitor;
    return 0;
}

int GetMonitorRefreshRate(int monitor)
{
    (void)monitor;
    return 60;
}

Vector2 GetWindowPosition(void)
{
    return (Vector2){0.0f, 0.0f};
}

const char *GetMonitorName(int monitor)
{
    (void)monitor;
    return "HTML5 Canvas";
}

void SetTraceLogLevel(int logLevel)
{
    (void)logLevel;
}

void SetExitKey(int key)
{
    g_exit_key = key;
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
