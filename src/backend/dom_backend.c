/*
 * dom_backend.c - HTML/CSS DOM backend for Emscripten web builds.
 *
 * This backend keeps Kryon/app logic in WASM and presents UI-oriented drawing
 * as absolutely positioned DOM nodes. Unsupported raylib surface areas are
 * supplied by the generated weak null backend.
 */

#ifdef __EMSCRIPTEN__

#include "kry_input.h"
#include "kryon.h"

#include <dirent.h>
#include <emscripten.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern unsigned char *kry_decode_image_rgba(const unsigned char *data, int len,
                                            int *width, int *height);

/* ------------------------------------------------------------------------- */
/* JS DOM host                                                               */
/* ------------------------------------------------------------------------- */

EM_JS(void, js_dom_boot, (int w, int h, const char *title), {
    var g = globalThis;
    var doc = typeof document !== 'undefined' ? document : null;
    var K = g.__kryDom = {
        w: w, h: h, renderW: w, renderH: h,
        dpi: Math.max(1, g.devicePixelRatio || 1),
        root: null, host: null, nodes: [], op: 0, frame: 0,
        nextSemantic: null,
        route: "", routeVersion: 0,
        clips: [], transforms: [""],
        textures: {}, nextTex: 1,
        fonts: {1: {family: 'Arial, sans-serif', size: 16}},
        nextFont: 2,
        keysDown: {}, keysPressed: [], keysReleased: [], keysRepeated: [],
        chars: [],
        mouseX: 0, mouseY: 0, mouseDeltaX: 0, mouseDeltaY: 0,
        mouseOffsetX: 0, mouseOffsetY: 0, mouseScaleX: 1, mouseScaleY: 1,
        buttonsDown: {}, buttonsPressed: [], buttonsReleased: [],
        wheelX: 0, wheelY: 0,
        touches: [], dropped: [], droppedPending: 0, focused: 1, resized: 0,
        clipboard: "", clipboardGestureUntil: 0
    };
    K.col = function (r, gg, b, a) {
        return 'rgba(' + r + ',' + gg + ',' + b + ',' + (a / 255.0) + ')';
    };
    K.ensureRoot = function () {
        if (!doc) return null;
        var host = doc.getElementById('canvas-frame') || null;
        var canvas = doc.getElementById('canvas') || null;
        var root = doc.getElementById('kryon-dom-root');
        if (!root) {
            root = doc.createElement('div');
            root.id = 'kryon-dom-root';
            (host || doc.body).appendChild(root);
        }
        if (canvas && canvas.style) canvas.style.display = 'none';
        if (host && host.style && !host.style.position)
            host.style.position = 'relative';
        root.tabIndex = 0;
        root.style.position = host ? 'absolute' : 'relative';
        root.style.inset = host ? '0' : "";
        root.style.overflow = 'hidden';
        root.style.width = host ? '100%' : K.w + 'px';
        root.style.height = host ? '100%' : K.h + 'px';
        root.style.touchAction = 'none';
        root.style.userSelect = 'none';
        root.style.contain = 'layout paint size';
        root.style.fontFamily = 'Arial, sans-serif';
        root.style.lineHeight = '1';
        K.root = root;
        K.host = host;
        if (host && host.getBoundingClientRect) {
            var r = host.getBoundingClientRect();
            if (r.width > 0 && r.height > 0) {
                K.w = Math.round(r.width);
                K.h = Math.round(r.height);
                K.renderW = Math.max(1, Math.round(K.w * K.dpi));
                K.renderH = Math.max(1, Math.round(K.h * K.dpi));
            }
        }
        return root;
    };
    K.resize = function (nw, nh) {
        K.w = Math.max(1, nw | 0);
        K.h = Math.max(1, nh | 0);
        K.dpi = Math.max(1, g.devicePixelRatio || 1);
        K.renderW = Math.max(1, Math.round(K.w * K.dpi));
        K.renderH = Math.max(1, Math.round(K.h * K.dpi));
        if (K.root && !K.host) {
            K.root.style.width = K.w + 'px';
            K.root.style.height = K.h + 'px';
        }
        K.resized = 1;
    };
    K.semanticName = function (kind) {
        switch (kind | 0) {
        case 1: return 'page';
        case 2: return 'section';
        case 3: return 'heading';
        case 4: return 'paragraph';
        case 5: return 'link';
        case 6: return 'picture';
        case 7: return 'button';
        default: return "";
        }
    };
    K.takeSemantic = function () {
        var meta = K.nextSemantic;
        K.nextSemantic = null;
        return meta;
    };
    K.applySemantic = function (n, meta) {
        if (!n) return;
        n.removeAttribute('role');
        n.removeAttribute('aria-label');
        n.removeAttribute('href');
        n.removeAttribute('target');
        n.removeAttribute('rel');
        n.removeAttribute('alt');
        if (!meta) {
            n.dataset.krySemantic = "";
            return;
        }
        var name = K.semanticName(meta.kind);
        n.dataset.krySemantic = name;
        if (meta.label) n.setAttribute('aria-label', meta.label);
        if (meta.role) n.setAttribute('role', meta.role);
        if (meta.kind === 3) n.setAttribute('role', 'heading');
        if (meta.kind === 5 && meta.href) {
            n.setAttribute('href', meta.href);
            if (meta.href.indexOf('://') >= 0) {
                n.setAttribute('target', '_blank');
                n.setAttribute('rel', 'noopener noreferrer');
            }
        }
        if (meta.kind === 6 && meta.label) n.setAttribute('alt', meta.label);
        if (meta.tabIndex >= 0) n.tabIndex = meta.tabIndex;
    };
    K.textTag = function (meta) {
        if (!meta) return 'div';
        if (meta.kind === 3) {
            var level = Math.max(1, Math.min(6, meta.level | 0));
            return 'h' + level;
        }
        if (meta.kind === 4) return 'p';
        if (meta.kind === 5) return 'a';
        return 'div';
    };
    K.ensureMeta = function (name) {
        if (!doc) return null;
        var selector = name === 'canonical'
            ? 'link[rel="canonical"]'
            : 'meta[name="' + name + '"]';
        var el = doc.querySelector ? doc.querySelector(selector) : null;
        if (!el) {
            el = doc.createElement(name === 'canonical' ? 'link' : 'meta');
            if (name === 'canonical') el.setAttribute('rel', 'canonical');
            else el.setAttribute('name', name);
            (doc.head || doc.documentElement || doc.body).appendChild(el);
        }
        return el;
    };
    K.routeText = function () {
        var loc = typeof location !== 'undefined' ? location : null;
        if (!loc) return '/';
        return (loc.pathname || '/') + (loc.hash || "");
    };
    K.noteRouteChange = function () {
        var next = K.routeText();
        if (next !== K.route) {
            K.route = next;
            K.routeVersion++;
        }
        return K.routeVersion;
    };
    K.node = function (tag, kind) {
        var i = K.op++;
        var n = K.nodes[i];
        if (!doc || !K.root) return null;
        if (!n || n.tagName.toLowerCase() !== tag) {
            if (n && n.parentNode) n.parentNode.removeChild(n);
            n = doc.createElement(tag);
            K.root.appendChild(n);
            K.nodes[i] = n;
        }
        n.dataset.kryKind = kind || tag;
        n.style.display = 'block';
        n.style.position = 'absolute';
        n.style.boxSizing = 'border-box';
        n.style.margin = '0';
        n.style.padding = '0';
        n.style.pointerEvents = 'none';
        n.style.transformOrigin = '0 0';
        n.style.opacity = "";
        n.textContent = "";
        n.removeAttribute('aria-hidden');
        n.removeAttribute('role');
        n.removeAttribute('aria-label');
        n.removeAttribute('href');
        n.removeAttribute('target');
        n.removeAttribute('rel');
        n.removeAttribute('alt');
        return n;
    };
    K.begin = function () {
        K.op = 0;
        K.clips = [];
        K.transforms = [""];
    };
    K.finish = function () {
        if (!K.root) return;
        for (var i = K.op; i < K.nodes.length; i++) {
            if (K.nodes[i]) K.nodes[i].style.display = 'none';
        }
        K.frame++;
    };
    K.baseStyle = function (n, x, y, w, h) {
        if (!n) return;
        n.style.left = x + 'px';
        n.style.top = y + 'px';
        n.style.width = Math.max(0, w) + 'px';
        n.style.height = Math.max(0, h) + 'px';
        n.style.border = '0';
        n.style.borderRadius = '0';
        n.style.background = 'transparent';
        n.style.color = "";
        n.style.overflow = 'visible';
        n.style.whiteSpace = 'normal';
        n.style.transform = K.transforms[K.transforms.length - 1] || "";
        if (K.clips.length) {
            var c = K.clips[K.clips.length - 1];
            var top = Math.max(0, c.y - y);
            var left = Math.max(0, c.x - x);
            var right = Math.max(0, (x + w) - (c.x + c.w));
            var bottom = Math.max(0, (y + h) - (c.y + c.h));
            n.style.clipPath = 'inset(' + top + 'px ' + right + 'px ' +
                bottom + 'px ' + left + 'px)';
        } else {
            n.style.clipPath = 'none';
        }
    };
    K.textWidth = function (fontId, text, size) {
        var f = K.fonts[fontId] || K.fonts[1];
        if (doc && doc.body) {
            var s = K.measureSpan || doc.createElement('span');
            K.measureSpan = s;
            s.style.position = 'fixed';
            s.style.left = '-10000px';
            s.style.top = '-10000px';
            s.style.visibility = 'hidden';
            s.style.whiteSpace = 'pre';
            s.style.fontFamily = f.family;
            s.style.fontSize = size + 'px';
            s.style.lineHeight = '1';
            s.textContent = text || "";
            if (!s.parentNode) doc.body.appendChild(s);
            return Math.ceil(s.getBoundingClientRect().width);
        }
        return Math.ceil((text || "").length * size * 0.56);
    };
    K.ensureRoot();
    K.route = K.routeText();
    if (g.addEventListener) {
        g.addEventListener('popstate', K.noteRouteChange);
        g.addEventListener('hashchange', K.noteRouteChange);
    }
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
        MetaLeft: 346, MetaRight: 347
    };
    var keyOf = function (code, key) {
        if (!code && key && key.length === 1) return key.toUpperCase().charCodeAt(0);
        if (code && code.startsWith('Key') && code.length === 4)
            return code.charCodeAt(3);
        if (code && code.startsWith('Digit') && code.length === 6)
            return code.charCodeAt(5);
        return KEYMAP[code] !== undefined ? KEYMAP[code] : 0;
    };
    var localPoint = function (e) {
        var r = K.root && K.root.getBoundingClientRect
            ? K.root.getBoundingClientRect() : {left: 0, top: 0};
        return {x: (e.clientX - r.left - K.mouseOffsetX) * K.mouseScaleX,
                y: (e.clientY - r.top - K.mouseOffsetY) * K.mouseScaleY};
    };
    var setMouse = function (x, y) {
        K.mouseDeltaX += x - K.mouseX;
        K.mouseDeltaY += y - K.mouseY;
        K.mouseX = x; K.mouseY = y;
    };
    var claimEvent = function (e, name) {
        var key = '__kryDomHandled_' + name;
        if (!e || e[key]) return false;
        try { e[key] = 1; } catch (_) {}
        return true;
    };
    var syncTouches = function (list) {
        var had = K.touches.length > 0;
        K.touches = [];
        for (var i = 0; i < list.length && i < 8; i++) {
            var p = localPoint(list[i]);
            K.touches.push({id: list[i].identifier | 0, x: p.x, y: p.y});
        }
        if (K.touches.length > 0) setMouse(K.touches[0].x, K.touches[0].y);
        if (!had && K.touches.length > 0) {
            K.buttonsDown[0] = 1;
            K.buttonsPressed.push(0);
        } else if (had && K.touches.length === 0) {
            delete K.buttonsDown[0];
            K.buttonsReleased.push(0);
        }
    };
    var hook = function (target) {
        if (!target || target.__kryDomHooked || !target.addEventListener)
            return;
        target.__kryDomHooked = 1;
        target.addEventListener('mousemove', function (e) {
            if (!claimEvent(e, 'mousemove')) return;
            var p = localPoint(e); setMouse(p.x, p.y);
        });
        target.addEventListener('mousedown', function (e) {
            if (!claimEvent(e, 'mousedown')) return;
            var p = localPoint(e); setMouse(p.x, p.y);
            if (K.root && K.root.focus) {
                try { K.root.focus({preventScroll: true}); } catch (_) {
                    try { K.root.focus(); } catch (_) {}
                }
            }
            K.buttonsDown[e.button] = 1;
            K.buttonsPressed.push(e.button);
        });
        target.addEventListener('mouseup', function (e) {
            if (!claimEvent(e, 'mouseup')) return;
            var p = localPoint(e); setMouse(p.x, p.y);
            delete K.buttonsDown[e.button];
            K.buttonsReleased.push(e.button);
        });
        target.addEventListener('wheel', function (e) {
            if (!claimEvent(e, 'wheel')) return;
            var unit = e.deltaMode === 1 ? 16 : (e.deltaMode === 2 ? K.h : 120);
            K.wheelX += -e.deltaX / unit;
            K.wheelY += -e.deltaY / unit;
            if (e.cancelable) e.preventDefault();
        });
        target.addEventListener('keydown', function (e) {
            if (!claimEvent(e, 'keydown')) return;
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                if (K.keysDown[k] && e.repeat) K.keysRepeated.push(k);
                else if (!K.keysDown[k]) K.keysPressed.push(k);
                K.keysDown[k] = 1;
                if ((e.ctrlKey || e.metaKey) && (k === 65 || k === 67 ||
                    k === 86 || k === 88)) {
                    var nowMs = (typeof performance !== 'undefined' &&
                        performance.now) ? performance.now() : Date.now();
                    K.clipboardGestureUntil = nowMs + 1000;
                }
                if (k === 32 || (k >= 256 && k <= 269)) e.preventDefault();
            }
        });
        target.addEventListener('keyup', function (e) {
            if (!claimEvent(e, 'keyup')) return;
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                delete K.keysDown[k];
                K.keysReleased.push(k);
            }
        });
        target.addEventListener('keypress', function (e) {
            if (!claimEvent(e, 'keypress')) return;
            if (e.key && e.key.length === 1) K.chars.push(e.key.charCodeAt(0));
        });
        target.addEventListener('touchstart', function (e) {
            if (!claimEvent(e, 'touchstart')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchmove', function (e) {
            if (!claimEvent(e, 'touchmove')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchend', function (e) {
            if (!claimEvent(e, 'touchend')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        target.addEventListener('touchcancel', function (e) {
            if (!claimEvent(e, 'touchcancel')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
    };
    hook(K.root);
    hook(doc);
    hook(g);
    if (g.addEventListener) {
        g.addEventListener('focus', function () { K.focused = 1; });
        g.addEventListener('blur', function () { K.focused = 0; });
        g.addEventListener('paste', function (e) {
            var data = e.clipboardData || (g.clipboardData || null);
            var text = data && data.getData ? data.getData('text') : "";
            if (text) K.clipboard = text;
        });
    }
    if ((K.host || K.root) && g.ResizeObserver) {
        K.ro = new ResizeObserver(function (entries) {
            var cr = entries && entries[0] && entries[0].contentRect;
            if (cr && cr.width > 0 && cr.height > 0 &&
                (Math.round(cr.width) !== K.w || Math.round(cr.height) !== K.h))
                K.resize(Math.round(cr.width), Math.round(cr.height));
        });
        try { K.ro.observe(K.host || K.root); } catch (_) {}
    }
});

EM_JS(void, js_dom_resize, (int w, int h), {
    var K = globalThis.__kryDom;
    if (K && K.resize) K.resize(w, h);
});

EM_JS(int, js_dom_dim, (int which), {
    var K = globalThis.__kryDom;
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

EM_JS(double, js_dom_dpi, (void), {
    var K = globalThis.__kryDom;
    return K ? K.dpi : 1.0;
});

EM_JS(void, js_dom_begin_frame, (void), {
    var K = globalThis.__kryDom;
    if (K) K.begin();
});

EM_JS(void, js_dom_end_frame, (void), {
    var K = globalThis.__kryDom;
    if (K) K.finish();
});

EM_JS(void, js_dom_clear, (int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    if (!K || !K.root) return;
    K.root.style.background = K.col(r, gg, b, a);
});

EM_JS(void, js_dom_rect, (int outline, double x, double y, double w, double h,
                          double radius, double thick,
                          int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var n = K && K.node ? K.node('div', outline ? 'rect-outline' : 'rect') : null;
    if (!n) return;
    K.baseStyle(n, x, y, w, h);
    n.style.borderRadius = Math.max(0, radius) + 'px';
    if (outline) {
        n.style.border = Math.max(1, thick) + 'px solid ' + K.col(r, gg, b, a);
    } else {
        n.style.background = K.col(r, gg, b, a);
    }
});

EM_JS(void, js_dom_gradient_rect, (int mode, double x, double y,
                                   double w, double h,
                                   int r1, int g1, int b1, int a1,
                                   int r2, int g2, int b2, int a2,
                                   int r3, int g3, int b3, int a3,
                                   int r4, int g4, int b4, int a4), {
    var K = globalThis.__kryDom;
    var n = K && K.node ? K.node('div', 'rect-gradient') : null;
    if (!n) return;
    K.baseStyle(n, x, y, w, h);
    if (mode === 1) {
        n.style.background = 'linear-gradient(to bottom, ' +
            K.col(r1, g1, b1, a1) + ', ' + K.col(r2, g2, b2, a2) + ')';
    } else if (mode === 2) {
        n.style.background = 'linear-gradient(to right, ' +
            K.col(r1, g1, b1, a1) + ', ' + K.col(r2, g2, b2, a2) + ')';
    } else {
        n.style.background = 'linear-gradient(135deg, ' +
            K.col(r1, g1, b1, a1) + ' 0%, ' +
            K.col(r2, g2, b2, a2) + ' 33%, ' +
            K.col(r3, g3, b3, a3) + ' 66%, ' +
            K.col(r4, g4, b4, a4) + ' 100%)';
    }
});


EM_JS(void, js_dom_line, (double x1, double y1, double x2, double y2,
                          double thick, int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var n = K && K.node ? K.node('div', 'line') : null;
    if (!n) return;
    var dx = x2 - x1, dy = y2 - y1;
    var len = Math.sqrt(dx * dx + dy * dy);
    var angle = Math.atan2(dy, dx) * 180 / Math.PI;
    K.baseStyle(n, x1, y1 - Math.max(1, thick) * 0.5, len, Math.max(1, thick));
    n.style.background = K.col(r, gg, b, a);
    n.style.transform = (K.transforms[K.transforms.length - 1] || "") +
        ' rotate(' + angle + 'deg)';
});

EM_JS(void, js_dom_circle, (int outline, double cx, double cy, double radius,
                            double thick, int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var n = K && K.node ? K.node('div', outline ? 'circle-outline' : 'circle') : null;
    if (!n) return;
    K.baseStyle(n, cx - radius, cy - radius, radius * 2, radius * 2);
    n.style.borderRadius = '50%';
    if (outline) {
        n.style.border = Math.max(1, thick) + 'px solid ' + K.col(r, gg, b, a);
    } else {
        n.style.background = K.col(r, gg, b, a);
    }
});

EM_JS(void, js_dom_text, (int font_id, const char *ptr, int byte_len,
                          double x, double y, double size,
                          int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var meta = K && K.takeSemantic ? K.takeSemantic() : null;
    var n = K && K.node ? K.node(K.textTag(meta), 'text') : null;
    if (!n) return;
    var text = ptr ? UTF8ToString(ptr, byte_len >= 0 ? byte_len : undefined) : "";
    var f = K.fonts[font_id] || K.fonts[1];
    var width = K.textWidth(font_id, text, size);
    K.baseStyle(n, x, y, width, Math.ceil(size * 1.25));
    K.applySemantic(n, meta);
    n.style.color = K.col(r, gg, b, a);
    n.style.fontFamily = f.family;
    n.style.fontSize = size + 'px';
    n.style.lineHeight = '1';
    n.style.whiteSpace = 'pre';
    n.style.userSelect = 'text';
    n.textContent = text;
});

EM_JS(int, js_dom_measure_text, (int font_id, const char *ptr, int byte_len,
                                 int size), {
    var K = globalThis.__kryDom;
    if (!K) return 0;
    var text = ptr ? UTF8ToString(ptr, byte_len >= 0 ? byte_len : undefined) : "";
    return K.textWidth(font_id, text, size) | 0;
});

EM_ASYNC_JS(int, js_dom_font_load, (int ptr, int len, int size), {
    var K = globalThis.__kryDom;
    if (!K) return 0;
    var id = K.nextFont++;
    var family = 'kry-dom-face-' + id;
    if (typeof FontFace !== 'undefined' && ptr && len > 0) {
        try {
            var buf = new Uint8Array(HEAPU8.subarray(ptr, ptr + len));
            var face = new FontFace(family, buf);
            await face.load();
            if (globalThis.document && document.fonts) document.fonts.add(face);
        } catch (e) {
            family = 'Arial, sans-serif';
        }
    } else {
        family = 'Arial, sans-serif';
    }
    K.fonts[id] = {family: family, size: size > 0 ? size : 16};
    return id;
});

EM_JS(void, js_dom_font_free, (int id), {
    var K = globalThis.__kryDom;
    if (K && id > 1) delete K.fonts[id];
});

EM_JS(void, js_dom_set_cursor, (int cursor), {
    var K = globalThis.__kryDom;
    var value = 'default';
    switch (cursor) {
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
    if (K && K.root) K.root.style.cursor = value;
    if (typeof document !== 'undefined') {
        document.body.style.cursor = value;
        document.documentElement.style.cursor = value;
    }
});

EM_JS(void, js_dom_set_title, (const char *title), {
    if (typeof document !== 'undefined' && document.title !== undefined)
        document.title = title ? UTF8ToString(title) : "";
});

EM_JS(void, js_dom_focus, (void), {
    var K = globalThis.__kryDom;
    if (K && K.root && K.root.focus) K.root.focus();
});

EM_JS(void, js_dom_fullscreen, (int enable), {
    var K = globalThis.__kryDom;
    if (typeof document === 'undefined' || !K || !K.root) return;
    if (enable) {
        if (!document.fullscreenElement && K.root.requestFullscreen)
            K.root.requestFullscreen().catch(function () {});
    } else if (document.fullscreenElement && document.exitFullscreen) {
        document.exitFullscreen().catch(function () {});
    }
});

EM_JS(int, js_dom_fullscreen_state, (void), {
    if (typeof document === 'undefined') return 0;
    return document.fullscreenElement ? 1 : 0;
});

EM_JS(void, js_dom_clip_push, (double x, double y, double w, double h), {
    var K = globalThis.__kryDom;
    if (K) K.clips.push({x: x, y: y, w: w, h: h});
});

EM_JS(void, js_dom_clip_pop, (void), {
    var K = globalThis.__kryDom;
    if (K && K.clips.length) K.clips.pop();
});

EM_JS(void, js_dom_transform_push, (double zoom, double ox, double oy,
                                    double tx, double ty, double rot), {
    var K = globalThis.__kryDom;
    if (!K) return;
    var base = K.transforms[K.transforms.length - 1] || "";
    var t = base + ' translate(' + ox + 'px,' + oy + 'px)' +
        ' rotate(' + (-rot) + 'deg)' +
        ' scale(' + zoom + ')' +
        ' translate(' + (-tx) + 'px,' + (-ty) + 'px)';
    K.transforms.push(t);
});

EM_JS(void, js_dom_transform_pop, (void), {
    var K = globalThis.__kryDom;
    if (K && K.transforms.length > 1) K.transforms.pop();
});

EM_JS(int, js_dom_texture_color, (int w, int h, int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    if (!K) return 0;
    var id = K.nextTex++;
    K.textures[id] = {w: w, h: h, color: K.col(r, gg, b, a)};
    return id;
});

EM_JS(int, js_dom_texture_rgba, (int ptr, int w, int h), {
    var K = globalThis.__kryDom;
    if (!K || !ptr || w <= 0 || h <= 0) return 0;
    var header = 54;
    var pixels = w * h * 4;
    var total = header + pixels;
    var bytes = new Uint8Array(total);
    var view = new DataView(bytes.buffer);
    bytes[0] = 0x42;
    bytes[1] = 0x4d;
    view.setUint32(2, total, true);
    view.setUint32(10, header, true);
    view.setUint32(14, 40, true);
    view.setInt32(18, w, true);
    view.setInt32(22, -h, true);
    view.setUint16(26, 1, true);
    view.setUint16(28, 32, true);
    view.setUint32(34, pixels, true);
    for (var i = 0; i < w * h; i++) {
        var s = ptr + i * 4;
        var d = header + i * 4;
        bytes[d + 0] = HEAPU8[s + 2];
        bytes[d + 1] = HEAPU8[s + 1];
        bytes[d + 2] = HEAPU8[s + 0];
        bytes[d + 3] = HEAPU8[s + 3];
    }
    var binary = "";
    var chunk = 0x8000;
    for (var off = 0; off < bytes.length; off += chunk) {
        var part = bytes.subarray(off, Math.min(off + chunk, bytes.length));
        binary += String.fromCharCode.apply(null, part);
    }
    var id = K.nextTex++;
    K.textures[id] = {
        w: w,
        h: h,
        url: 'data:image/bmp;base64,' + btoa(binary)
    };
    return id;
});

EM_JS(void, js_dom_texture_free, (int id), {
    var K = globalThis.__kryDom;
    if (K) delete K.textures[id];
});

EM_JS(void, js_dom_texture_draw, (int id, double x, double y, double w,
                                  double h, double ox, double oy, double rot,
                                  int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var tex = K && K.textures ? K.textures[id] : null;
    var meta = K && K.takeSemantic ? K.takeSemantic() : null;
    var n = K && K.node ? K.node(tex && tex.url ? 'img' : 'div', 'texture') : null;
    if (!n || !tex) return;
    K.baseStyle(n, x, y, w, h);
    K.applySemantic(n, meta);
    if (tex.url) {
        if (n.src !== tex.url) n.src = tex.url;
        n.alt = meta && meta.label ? meta.label : "";
        n.style.objectFit = 'fill';
        n.style.background = 'transparent';
    } else {
        n.style.background = tex.color || K.col(r, gg, b, a);
    }
    n.style.opacity = String(a / 255.0);
    n.style.transform = (K.transforms[K.transforms.length - 1] || "") +
        ' translate(' + ox + 'px,' + oy + 'px) rotate(' + rot + 'deg)' +
        ' translate(' + (-ox) + 'px,' + (-oy) + 'px)';
});

EM_JS(void, js_dom_semantic_next, (int kind, const char *label,
                                   const char *href, const char *role,
                                   int level, int tab_index), {
    var K = globalThis.__kryDom;
    if (!K) return;
    K.nextSemantic = {
        kind: kind | 0,
        label: label ? UTF8ToString(label) : "",
        href: href ? UTF8ToString(href) : "",
        role: role ? UTF8ToString(role) : "",
        level: level | 0,
        tabIndex: tab_index | 0
    };
});

EM_JS(void, js_dom_semantic_box, (int kind, double x, double y, double w,
                                  double h, const char *label), {
    var K = globalThis.__kryDom;
    if (!K || !K.node) return;
    var tag = kind === 1 ? 'main' : (kind === 2 ? 'section' : 'div');
    var n = K.node(tag, K.semanticName(kind) || 'semantic');
    if (!n) return;
    K.baseStyle(n, x, y, w, h);
    K.applySemantic(n, {
        kind: kind | 0,
        label: label ? UTF8ToString(label) : "",
        href: "",
        role: kind === 1 ? "main" : (kind === 2 ? "region" : ""),
        level: 0,
        tabIndex: -1
    });
    n.style.background = 'transparent';
    n.style.pointerEvents = 'none';
});

EM_JS(void, js_dom_page_meta, (int which, const char *value,
                               int r, int gg, int b, int a), {
    var K = globalThis.__kryDom;
    var doc = typeof document !== 'undefined' ? document : null;
    if (!K || !doc) return;
    var text = value ? UTF8ToString(value) : "";
    if (which === 0) {
        doc.title = text;
    } else if (which === 1) {
        var desc = K.ensureMeta('description');
        if (desc) desc.setAttribute('content', text);
    } else if (which === 2) {
        var canonical = K.ensureMeta('canonical');
        if (canonical) canonical.setAttribute('href', text);
    } else if (which === 3) {
        var theme = K.ensureMeta('theme-color');
        var hex = '#' + [r, gg, b].map(function (v) {
            return Math.max(0, Math.min(255, v | 0)).toString(16).padStart(2, '0');
        }).join("");
        if (theme) theme.setAttribute('content', a > 0 ? hex : "");
    }
});

EM_JS(void, js_dom_route_set, (int replace, const char *path), {
    var K = globalThis.__kryDom;
    var value = path ? UTF8ToString(path) : "";
    if (!value || typeof history === 'undefined') return;
    try {
        if (replace) history.replaceState({}, "", value);
        else history.pushState({}, "", value);
        if (K && K.noteRouteChange) K.noteRouteChange();
    } catch (_) {}
});

EM_JS(void, js_dom_route_get, (int which, char *dst, int cap), {
    if (!dst || cap <= 0) return;
    var loc = typeof location !== 'undefined' ? location : null;
    var text = "";
    if (loc) text = which === 0 ? (loc.pathname || "/") : (loc.hash || "");
    stringToUTF8(text, dst, cap);
});

EM_JS(int, js_dom_route_version, (void), {
    var K = globalThis.__kryDom;
    if (!K) return 0;
    if (K.noteRouteChange) K.noteRouteChange();
    return K.routeVersion | 0;
});

void kry_dom_semantic_next(int kind, const char *label, const char *href,
                           const char *role, int level, int tab_index)
{
    js_dom_semantic_next(kind, label, href, role, level, tab_index);
}

void kry_dom_semantic_box(int kind, Rectangle bounds, const char *label)
{
    js_dom_semantic_box(kind, bounds.x, bounds.y, bounds.width, bounds.height,
                        label);
}

void kry_dom_set_page_title(const char *title)
{
    js_dom_page_meta(0, title, 0, 0, 0, 0);
}

void kry_dom_set_page_description(const char *description)
{
    js_dom_page_meta(1, description, 0, 0, 0, 0);
}

void kry_dom_set_page_canonical_url(const char *url)
{
    js_dom_page_meta(2, url, 0, 0, 0, 0);
}

void kry_dom_set_page_theme_color(Color color)
{
    js_dom_page_meta(3, NULL, color.r, color.g, color.b, color.a);
}

const char *kry_dom_get_route_path(void)
{
    static char path[1024];

    js_dom_route_get(0, path, (int)sizeof(path));
    return path[0] != '\0' ? path : "/";
}

const char *kry_dom_get_route_hash(void)
{
    static char hash[1024];

    js_dom_route_get(1, hash, (int)sizeof(hash));
    return hash;
}

int kry_dom_get_route_version(void) { return js_dom_route_version(); }

void kry_dom_push_route(const char *path)
{
    js_dom_route_set(0, path);
}

void kry_dom_replace_route(const char *path)
{
    js_dom_route_set(1, path);
}

EM_JS(int, js_dom_input_query, (int which, int code), {
    var K = globalThis.__kryDom;
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
    case 8: return K.mouseX | 0;
    case 9: return K.mouseY | 0;
    case 10: return K.mouseDeltaX | 0;
    case 11: return K.mouseDeltaY | 0;
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

EM_JS(double, js_dom_input_float, (int which), {
    var K = globalThis.__kryDom;
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

EM_JS(void, js_dom_input_set_mouse, (int x, int y), {
    var K = globalThis.__kryDom;
    if (!K) return;
    K.mouseDeltaX += x - K.mouseX;
    K.mouseDeltaY += y - K.mouseY;
    K.mouseX = x;
    K.mouseY = y;
});

EM_JS(void, js_dom_input_mouse_config, (double ox, double oy,
                                        double sx, double sy), {
    var K = globalThis.__kryDom;
    if (!K) return;
    K.mouseOffsetX = ox;
    K.mouseOffsetY = oy;
    K.mouseScaleX = sx;
    K.mouseScaleY = sy;
});

EM_JS(int, js_dom_touch_query, (int index, int field), {
    var K = globalThis.__kryDom;
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

EM_JS(void, js_dom_clipboard_pull, (char *dst, int cap), {
    var K = globalThis.__kryDom;
    if (!K || !dst || cap <= 0) return;
    stringToUTF8(K.clipboard || "", dst, cap);
});

EM_JS(void, js_dom_clipboard_push, (const char *text), {
    var value = text ? UTF8ToString(text) : "";
    var K = globalThis.__kryDom;
    if (K) K.clipboard = value;
    if (globalThis.navigator && navigator.clipboard && navigator.clipboard.writeText)
        navigator.clipboard.writeText(value).catch(function () {});
});

EM_JS(int, js_dom_dropped_count, (void), {
    var K = globalThis.__kryDom;
    if (K && K.droppedPending > 0) return 0;
    return K && K.dropped ? K.dropped.length : 0;
});

EM_JS(char *, js_dom_dropped_path, (int index), {
    var K = globalThis.__kryDom;
    var path = K && K.dropped && index >= 0 && index < K.dropped.length ?
        K.dropped[index] : "";
    var len = lengthBytesUTF8(path) + 1;
    var ptr = _malloc(len);
    stringToUTF8(path, ptr, len);
    return ptr;
});

/* ------------------------------------------------------------------------- */
/* Window/frame                                                              */
/* ------------------------------------------------------------------------- */

static int g_win_w;
static int g_win_h;
static int g_win_ready;
static int g_exit_key = KEY_ESCAPE;
static unsigned int g_window_state;
static double g_last_frame;
static float g_frame_time = 1.0f / 60.0f;

void KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    g_win_w = width;
    g_win_h = height;
    js_dom_boot(width, height, title);
    g_win_ready = 1;
    g_last_frame = emscripten_get_now() / 1000.0;
}

void KryonRaylibBackend_CloseWindow(void)
{
    g_win_ready = 0;
}

bool KryonRaylibBackend_WindowShouldClose(void)
{
    return g_exit_key != 0 && js_dom_input_query(1, g_exit_key) != 0;
}

bool IsWindowReady(void) { return g_win_ready != 0; }
bool IsWindowFocused(void) { return g_win_ready != 0 && js_dom_dim(5) != 0; }
bool IsWindowHidden(void) { return (g_window_state & FLAG_WINDOW_HIDDEN) != 0; }
bool IsWindowMinimized(void) { return (g_window_state & FLAG_WINDOW_MINIMIZED) != 0; }
bool IsWindowMaximized(void) { return (g_window_state & FLAG_WINDOW_MAXIMIZED) != 0; }
bool IsWindowResized(void) { return js_dom_dim(4) != 0; }
bool IsWindowState(unsigned int flag) { return (g_window_state & flag) != 0; }
bool IsWindowFullscreen(void)
{
    return ((g_window_state & FLAG_FULLSCREEN_MODE) != 0) ||
           js_dom_fullscreen_state() != 0;
}

void SetConfigFlags(unsigned int flags) { g_window_state |= flags; }
void SetWindowState(unsigned int flags)
{
    g_window_state |= flags;
    if((flags & FLAG_FULLSCREEN_MODE) != 0)
        js_dom_fullscreen(1);
}
void ClearWindowState(unsigned int flags)
{
    g_window_state &= ~flags;
    if((flags & FLAG_FULLSCREEN_MODE) != 0)
        js_dom_fullscreen(0);
}
void ToggleFullscreen(void)
{
    if(IsWindowFullscreen())
        ClearWindowState(FLAG_FULLSCREEN_MODE);
    else
        SetWindowState(FLAG_FULLSCREEN_MODE);
}
void ToggleBorderlessWindowed(void) { g_window_state ^= FLAG_BORDERLESS_WINDOWED_MODE; }
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
                        FLAG_FULLSCREEN_MODE | FLAG_BORDERLESS_WINDOWED_MODE);
}

void BeginDrawing(void)
{
    js_dom_begin_frame();
}

void KryonRaylibBackend_EndDrawing(void)
{
    double now = emscripten_get_now() / 1000.0;

    js_dom_end_frame();
    g_frame_time = (float)(now - g_last_frame);
    if(g_frame_time <= 0.0f)
        g_frame_time = 1.0f / 60.0f;
    g_last_frame = now;
    (void)js_dom_input_query(13, 0);
    (void)js_dom_input_query(14, 0);
    (void)js_dom_input_query(15, 0);
    (void)js_dom_input_query(16, 0);
    (void)js_dom_input_query(17, 0);
    (void)js_dom_input_query(18, 0);
    (void)js_dom_input_query(20, 0);
    emscripten_sleep(1);
}

void SetTargetFPS(int fps) { (void)fps; }
void SetWindowSize(int width, int height)
{
    g_win_w = width;
    g_win_h = height;
    js_dom_resize(width, height);
}
int GetScreenWidth(void)
{
    int w = js_dom_dim(0);
    return w > 0 ? w : g_win_w;
}
int GetScreenHeight(void)
{
    int h = js_dom_dim(1);
    return h > 0 ? h : g_win_h;
}
int GetRenderWidth(void)
{
    int w = js_dom_dim(2);
    return w > 0 ? w : GetScreenWidth();
}
int GetRenderHeight(void)
{
    int h = js_dom_dim(3);
    return h > 0 ? h : GetScreenHeight();
}
double GetTime(void) { return emscripten_get_now() / 1000.0; }
float GetFrameTime(void) { return g_frame_time; }
Vector2 GetWindowScaleDPI(void)
{
    float dpr = (float)js_dom_dpi();
    return (Vector2){dpr, dpr};
}
void SetMouseCursor(int cursor) { js_dom_set_cursor(cursor); }
void SetWindowTitle(const char *title) { js_dom_set_title(title); }
void SetWindowFocused(void) { js_dom_focus(); }
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height) { (void)width; (void)height; }
void SetWindowMaxSize(int width, int height) { (void)width; (void)height; }
void SetWindowOpacity(float opacity) { (void)opacity; }
void *GetWindowHandle(void) { return NULL; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){0}; }
int GetMonitorWidth(int monitor) { (void)monitor; return GetScreenWidth(); }
int GetMonitorHeight(int monitor) { (void)monitor; return GetScreenHeight(); }
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return 0; }
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return 0; }
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 60; }
Vector2 GetWindowPosition(void) { return (Vector2){0}; }
const char *GetMonitorName(int monitor) { (void)monitor; return "HTML/CSS DOM"; }
void SetExitKey(int key) { g_exit_key = key; }
void WaitTime(double seconds)
{
    if(seconds > 0.0)
        emscripten_sleep((unsigned int)(seconds * 1000.0));
}
const char *GetApplicationDirectory(void) { return ""; }

/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */

static double dom_radius(Rectangle rec, float roundness)
{
    double min_side = rec.width < rec.height ? rec.width : rec.height;
    if(roundness < 0.0f)
        roundness = 0.0f;
    if(roundness > 1.0f)
        roundness = 1.0f;
    return (double)min_side * 0.5 * (double)roundness;
}

void ClearBackground(Color color)
{
    js_dom_clear(color.r, color.g, color.b, color.a);
}

void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    js_dom_rect(0, posX, posY, width, height, 0, 0,
                color.r, color.g, color.b, color.a);
}
void DrawPixel(int posX, int posY, Color color) { DrawRectangle(posX, posY, 1, 1, color); }
void DrawPixelV(Vector2 position, Color color) { DrawPixel((int)position.x, (int)position.y, color); }
void DrawRectangleRec(Rectangle rec, Color color)
{
    DrawRectangle((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, color);
}
void DrawRectangleV(Vector2 position, Vector2 size, Color color)
{
    DrawRectangle((int)position.x, (int)position.y, (int)size.x, (int)size.y, color);
}
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color)
{
    (void)origin;
    (void)rotation;
    DrawRectangleRec(rec, color);
}
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
    js_dom_rect(1, posX, posY, width, height, 0, 1,
                color.r, color.g, color.b, color.a);
}
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    js_dom_rect(1, rec.x, rec.y, rec.width, rec.height, 0, lineThick,
                color.r, color.g, color.b, color.a);
}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                          Color color)
{
    (void)segments;
    js_dom_rect(0, rec.x, rec.y, rec.width, rec.height,
                dom_radius(rec, roundness), 0,
                color.r, color.g, color.b, color.a);
}
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                               Color color)
{
    DrawRectangleRoundedLinesEx(rec, roundness, segments, 1.0f, color);
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                 float lineThick, Color color)
{
    (void)segments;
    js_dom_rect(1, rec.x, rec.y, rec.width, rec.height,
                dom_radius(rec, roundness), lineThick,
                color.r, color.g, color.b, color.a);
}
void DrawRectangleGradientV(int posX, int posY, int width, int height,
                            Color top, Color bottom)
{
    js_dom_gradient_rect(1, posX, posY, width, height,
                         top.r, top.g, top.b, top.a,
                         bottom.r, bottom.g, bottom.b, bottom.a,
                         0, 0, 0, 0, 0, 0, 0, 0);
}
void DrawRectangleGradientH(int posX, int posY, int width, int height,
                            Color left, Color right)
{
    js_dom_gradient_rect(2, posX, posY, width, height,
                         left.r, left.g, left.b, left.a,
                         right.r, right.g, right.b, right.a,
                         0, 0, 0, 0, 0, 0, 0, 0);
}
void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2,
                             Color col3, Color col4)
{
    js_dom_gradient_rect(3, rec.x, rec.y, rec.width, rec.height,
                         col1.r, col1.g, col1.b, col1.a,
                         col2.r, col2.g, col2.b, col2.a,
                         col3.r, col3.g, col3.b, col3.a,
                         col4.r, col4.g, col4.b, col4.a);
}
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
              Color color)
{
    js_dom_line(startPosX, startPosY, endPosX, endPosY, 1,
                color.r, color.g, color.b, color.a);
}
void DrawLineV(Vector2 startPos, Vector2 endPos, Color color)
{
    DrawLine((int)startPos.x, (int)startPos.y, (int)endPos.x, (int)endPos.y,
             color);
}
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    js_dom_line(startPos.x, startPos.y, endPos.x, endPos.y, thick,
                color.r, color.g, color.b, color.a);
}
void DrawLineStrip(const Vector2 *points, int pointCount, Color color)
{
    int i;
    if(points == NULL)
        return;
    for(i = 1; i < pointCount; i++)
        DrawLineV(points[i - 1], points[i], color);
}
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    js_dom_circle(0, centerX, centerY, radius, 0,
                  color.r, color.g, color.b, color.a);
}
void DrawCircleV(Vector2 center, float radius, Color color)
{
    DrawCircle((int)center.x, (int)center.y, radius, color);
}
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    js_dom_circle(1, centerX, centerY, radius, 1,
                  color.r, color.g, color.b, color.a);
}
void DrawCircleLinesV(Vector2 center, float radius, Color color)
{
    DrawCircleLines((int)center.x, (int)center.y, radius, color);
}
void DrawCircleLinesEx(Vector2 center, float radius, float lineThick,
                       Color color)
{
    js_dom_circle(1, center.x, center.y, radius, lineThick,
                  color.r, color.g, color.b, color.a);
}
void DrawRing(Vector2 center, float innerRadius, float outerRadius,
              float startAngle, float endAngle, int segments, Color color)
{
    (void)innerRadius;
    (void)startAngle;
    (void)endAngle;
    (void)segments;
    DrawCircleV(center, outerRadius, color);
}
void DrawRingLines(Vector2 center, float innerRadius, float outerRadius,
                   float startAngle, float endAngle, int segments, Color color)
{
    (void)innerRadius;
    (void)startAngle;
    (void)endAngle;
    (void)segments;
    DrawCircleLinesV(center, outerRadius, color);
}
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    DrawLineV(v1, v2, color);
    DrawLineV(v2, v3, color);
    DrawLineV(v3, v1, color);
}
void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    DrawTriangle(v1, v2, v3, color);
}
void DrawTriangleFan(const Vector2 *points, int pointCount, Color color)
{
    DrawLineStrip(points, pointCount, color);
}
void DrawTriangleStrip(const Vector2 *points, int pointCount, Color color)
{
    DrawLineStrip(points, pointCount, color);
}
void BeginScissorMode(int x, int y, int width, int height)
{
    js_dom_clip_push(x, y, width, height);
}
void EndScissorMode(void) { js_dom_clip_pop(); }
void BeginMode2D(Camera2D camera)
{
    js_dom_transform_push(camera.zoom <= 0.0f ? 1.0f : camera.zoom,
                          camera.offset.x, camera.offset.y,
                          camera.target.x, camera.target.y,
                          camera.rotation);
}
void EndMode2D(void) { js_dom_transform_pop(); }

/* ------------------------------------------------------------------------- */
/* Text/font surface                                                         */
/* ------------------------------------------------------------------------- */

static Font g_default_font;
static int g_default_font_ready;
#define DOM_FONT_TABLE_MAX 1024
static int g_dom_font_sizes[DOM_FONT_TABLE_MAX];

static int dom_glyph_index(const Font *font, int codepoint)
{
    int i;
    int question = -1;

    if(font == NULL || font->glyphs == NULL)
        return 0;
    for(i = 0; i < font->glyphCount; i++) {
        if(font->glyphs[i].value == codepoint)
            return i;
        if(font->glyphs[i].value == '?')
            question = i;
    }
    return question >= 0 ? question : 0;
}

static Font dom_make_font(unsigned id, int baseSize, const int *codepoints,
                          int glyphCount)
{
    Font font;
    int i;

    memset(&font, 0, sizeof(font));
    if(baseSize <= 0)
        baseSize = 16;
    if(codepoints == NULL || glyphCount <= 0) {
        static int default_set[95];

        for(i = 0; i < 95; i++)
            default_set[i] = 32 + i;
        codepoints = default_set;
        glyphCount = 95;
    }
    font.glyphs = calloc((size_t)glyphCount, sizeof(GlyphInfo));
    font.recs = calloc((size_t)glyphCount, sizeof(Rectangle));
    if(font.glyphs == NULL || font.recs == NULL) {
        free(font.glyphs);
        free(font.recs);
        memset(&font, 0, sizeof(font));
        return font;
    }
    font.baseSize = baseSize;
    font.glyphCount = glyphCount;
    font.glyphPadding = 0;
    font.texture.id = id;
    font.texture.width = 1;
    font.texture.height = 1;
    font.texture.mipmaps = 1;
    font.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    if(id < DOM_FONT_TABLE_MAX)
        g_dom_font_sizes[id] = baseSize;
    for(i = 0; i < glyphCount; i++) {
        char text[8];
        int bytes = 0;
        int cp = codepoints[i];
        int adv;

        if(cp <= 0)
            cp = '?';
        bytes = snprintf(text, sizeof(text), "%c", cp >= 32 && cp < 127 ? cp : '?');
        adv = js_dom_measure_text((int)id, text, bytes, baseSize);
        font.glyphs[i].value = cp;
        font.glyphs[i].advanceX = adv > 0 ? adv : (baseSize + 1) / 2;
        font.glyphs[i].offsetX = 0;
        font.glyphs[i].offsetY = 0;
        font.recs[i] = (Rectangle){0, 0, (float)font.glyphs[i].advanceX,
                                   (float)baseSize};
    }
    return font;
}

Font GetFontDefault(void)
{
    if(!g_default_font_ready) {
        g_default_font = dom_make_font(1, 16, NULL, 0);
        g_default_font_ready = IsFontValid(g_default_font);
    }
    return g_default_font;
}

Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize, int fontSize, const int *codepoints,
                        int glyphCount)
{
    unsigned id;

    (void)fileType;
    if(fileData == NULL || dataSize <= 0)
        return (Font){0};
    id = (unsigned)js_dom_font_load((int)(size_t)fileData, dataSize, fontSize);
    if(id == 0)
        return (Font){0};
    return dom_make_font(id, fontSize, codepoints, glyphCount);
}

Font LoadFont(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font = {0};

    if(data != NULL) {
        font = LoadFontFromMemory(".ttf", data, len, 16, NULL, 0);
        free(data);
    }
    return font;
}

Font LoadFontEx(const char *fileName, int fontSize, const int *codepoints,
                int codepointCount)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font = {0};

    if(data != NULL) {
        font = LoadFontFromMemory(".ttf", data, len, fontSize, codepoints,
                                  codepointCount);
        free(data);
    }
    return font;
}

void UnloadFont(Font font)
{
    free(font.glyphs);
    free(font.recs);
    if(font.texture.id < DOM_FONT_TABLE_MAX)
        g_dom_font_sizes[font.texture.id] = 0;
    if(font.texture.id > 1)
        js_dom_font_free((int)font.texture.id);
}

bool IsFontValid(Font font)
{
    return font.baseSize > 0 && font.glyphCount > 0 &&
           font.glyphs != NULL && font.recs != NULL &&
           font.texture.id != 0;
}

int GetGlyphIndex(Font font, int codepoint) { return dom_glyph_index(&font, codepoint); }
GlyphInfo GetGlyphInfo(Font font, int codepoint)
{
    if(!IsFontValid(font))
        return (GlyphInfo){0};
    return font.glyphs[dom_glyph_index(&font, codepoint)];
}
Rectangle GetGlyphAtlasRec(Font font, int codepoint)
{
    if(!IsFontValid(font))
        return (Rectangle){0};
    return font.recs[dom_glyph_index(&font, codepoint)];
}

GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize,
                        int fontSize, const int *codepoints,
                        int codepointCount, int type, int *glyphCount)
{
    Font font;
    GlyphInfo *copy;

    (void)type;
    if(glyphCount != NULL)
        *glyphCount = 0;
    font = LoadFontFromMemory(".ttf", fileData, dataSize, fontSize,
                              codepoints, codepointCount);
    if(!IsFontValid(font))
        return NULL;
    copy = malloc((size_t)font.glyphCount * sizeof(*copy));
    if(copy != NULL) {
        memcpy(copy, font.glyphs, (size_t)font.glyphCount * sizeof(*copy));
        if(glyphCount != NULL)
            *glyphCount = font.glyphCount;
    }
    UnloadFont(font);
    return copy;
}
void UnloadFontData(GlyphInfo *glyphs, int glyphCount)
{
    (void)glyphCount;
    free(glyphs);
}

int kry_dom_font_height(unsigned id)
{
    if(id == 0)
        return 0;
    if(id < DOM_FONT_TABLE_MAX && g_dom_font_sizes[id] > 0)
        return g_dom_font_sizes[id];
    return 16;
}
int kry_dom_font_text_width(unsigned id, const char *text, int byte_len)
{
    return js_dom_measure_text((int)id, text, byte_len, 16);
}
void kry_dom_queue_text(unsigned font_id, const char *text, int byte_len,
                        int x, int y, int font_size, Color color)
{
    js_dom_text((int)font_id, text, byte_len, x, y, font_size,
                color.r, color.g, color.b, color.a);
}

void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize,
                float spacing, Color tint)
{
    (void)spacing;
    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return;
    js_dom_text((int)font.texture.id, text, -1, position.x, position.y,
                fontSize, tint.r, tint.g, tint.b, tint.a);
}
void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
    DrawTextEx(GetFontDefault(), text, (Vector2){(float)posX, (float)posY},
               (float)fontSize, 0.0f, color);
}
void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin,
                 float rotation, float fontSize, float spacing, Color tint)
{
    (void)origin;
    (void)rotation;
    DrawTextEx(font, text, position, fontSize, spacing, tint);
}
void DrawTextCodepoint(Font font, int codepoint, Vector2 position,
                       float fontSize, Color tint)
{
    char text[8];
    if(codepoint >= 32 && codepoint < 127) {
        text[0] = (char)codepoint;
        text[1] = '\0';
    } else {
        strcpy(text, "?");
    }
    DrawTextEx(font, text, position, fontSize, 0.0f, tint);
}
void DrawTextCodepoints(Font font, const int *codepoints, int codepointCount,
                        Vector2 position, float fontSize, float spacing,
                        Color tint)
{
    Vector2 pen = position;
    int i;

    if(codepoints == NULL)
        return;
    for(i = 0; i < codepointCount; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);
        DrawTextCodepoint(font, codepoints[i], pen, fontSize, tint);
        pen.x += (float)glyph.advanceX + spacing;
    }
}
Vector2 MeasureTextEx(Font font, const char *text, float fontSize,
                      float spacing)
{
    int width;
    (void)spacing;
    if(!IsFontValid(font))
        font = GetFontDefault();
    width = js_dom_measure_text((int)font.texture.id, text, -1, (int)fontSize);
    return (Vector2){(float)width, fontSize};
}
int MeasureText(const char *text, int fontSize)
{
    return (int)MeasureTextEx(GetFontDefault(), text, (float)fontSize,
                              0.0f).x;
}
Vector2 MeasureTextCodepoints(Font font, const int *codepoints, int length,
                              float fontSize, float spacing)
{
    float width = 0.0f;
    int i;
    (void)spacing;
    if(!IsFontValid(font))
        font = GetFontDefault();
    if(codepoints == NULL)
        return (Vector2){0, fontSize};
    for(i = 0; i < length; i++)
        width += (float)GetGlyphInfo(font, codepoints[i]).advanceX;
    return (Vector2){width, fontSize};
}

/* ------------------------------------------------------------------------- */
/* Textures/images                                                           */
/* ------------------------------------------------------------------------- */

static Image dom_image_from_rgba(unsigned char *rgba, int w, int h)
{
    Image img;

    memset(&img, 0, sizeof(img));
    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}

Image GenImageColor(int width, int height, Color color)
{
    Image img = {0};
    unsigned char *px;
    int i;

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
    return dom_image_from_rgba(px, width, height);
}
bool IsImageValid(Image image)
{
    return image.data != NULL && image.width > 0 && image.height > 0;
}
void ImageFormat(Image *image, int newFormat)
{
    if(image != NULL)
        image->format = newFormat;
}
void ImageFlipVertical(Image *image)
{
    (void)image;
}
void UnloadImage(Image image)
{
    free(image.data);
}
Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex = {0};
    unsigned char *px = image.data;

    if(px == NULL || image.width <= 0 || image.height <= 0)
        return tex;
    tex.id = (unsigned)js_dom_texture_rgba((int)(size_t)px, image.width,
                                           image.height);
    if(tex.id == 0)
        tex.id = (unsigned)js_dom_texture_color(image.width, image.height,
                                                px[0], px[1], px[2], px[3]);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
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
        js_dom_texture_free((int)texture.id);
}
bool IsTextureValid(Texture2D texture)
{
    return texture.id != 0 && texture.width > 0 && texture.height > 0;
}
void SetTextureFilter(Texture2D texture, int filter)
{
    (void)texture;
    (void)filter;
}
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    int w = 0;
    int h = 0;
    unsigned char *rgba;

    (void)fileType;
    if(fileData == NULL || dataSize <= 0)
        return (Image){0};
    rgba = kry_decode_image_rgba(fileData, dataSize, &w, &h);
    if(rgba == NULL)
        return (Image){0};
    return dom_image_from_rgba(rgba, w, h);
}
Image LoadImage(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Image img;

    if(data == NULL)
        return (Image){0};
    img = LoadImageFromMemory(GetFileExtension(fileName), data, len);
    free(data);
    return img;
}
Image LoadImageFromTexture(Texture2D texture)
{
    (void)texture;
    return (Image){0};
}
int kry_backend_capture_screen(Image *image)
{
    if(image != NULL)
        memset(image, 0, sizeof(*image));
    return -1;
}
bool ExportImage(Image image, const char *fileName)
{
    (void)image;
    (void)fileName;
    return false;
}
RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D target = {0};
    Image img = GenImageColor(width, height, BLANK);

    target.texture = LoadTextureFromImage(img);
    target.id = target.texture.id;
    UnloadImage(img);
    return target;
}
void UnloadRenderTexture(RenderTexture2D target)
{
    UnloadTexture(target.texture);
}
bool IsRenderTextureValid(RenderTexture2D target)
{
    return target.id != 0 && IsTextureValid(target.texture);
}
void BeginTextureMode(RenderTexture2D target) { (void)target; }
void EndTextureMode(void) {}
void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width,
                               (float)texture.height},
                   (Rectangle){(float)posX, (float)posY,
                               (float)texture.width, (float)texture.height},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTextureV(Texture2D texture, Vector2 position, Color tint)
{
    DrawTexture(texture, (int)position.x, (int)position.y, tint);
}
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation,
                   float scale, Color tint)
{
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width,
                               (float)texture.height},
                   (Rectangle){position.x, position.y,
                               texture.width * scale,
                               texture.height * scale},
                   (Vector2){0, 0}, rotation, tint);
}
void DrawTextureRec(Texture2D texture, Rectangle rec, Vector2 position,
                    Color tint)
{
    DrawTexturePro(texture, rec,
                   (Rectangle){position.x, position.y,
                               rec.width < 0 ? -rec.width : rec.width,
                               rec.height < 0 ? -rec.height : rec.height},
                   (Vector2){0, 0}, 0.0f, tint);
}
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    (void)source;
    if(texture.id == 0)
        return;
    js_dom_texture_draw((int)texture.id, dest.x, dest.y, dest.width,
                        dest.height, origin.x, origin.y, rotation,
                        tint.r, tint.g, tint.b, tint.a);
}

/* ------------------------------------------------------------------------- */
/* Input                                                                     */
/* ------------------------------------------------------------------------- */

bool BackendRaw_IsKeyPressed(int key) { return js_dom_input_query(1, key) != 0; }
bool BackendRaw_IsKeyPressedRepeat(int key) { return js_dom_input_query(19, key) != 0; }
bool BackendRaw_IsKeyDown(int key) { return js_dom_input_query(0, key) != 0; }
bool BackendRaw_IsKeyReleased(int key) { return js_dom_input_query(2, key) != 0; }
int BackendRaw_GetKeyPressed(void) { return js_dom_input_query(3, 0); }
int BackendRaw_GetCharPressed(void) { return js_dom_input_query(4, 0); }
bool BackendRaw_IsMouseButtonPressed(int button) { return js_dom_input_query(6, button) != 0; }
bool BackendRaw_IsMouseButtonDown(int button) { return js_dom_input_query(5, button) != 0; }
bool BackendRaw_IsMouseButtonReleased(int button) { return js_dom_input_query(7, button) != 0; }
bool BackendRaw_IsMouseButtonUp(int button) { return !BackendRaw_IsMouseButtonDown(button); }
int BackendRaw_GetMouseX(void) { return js_dom_input_query(8, 0); }
int BackendRaw_GetMouseY(void) { return js_dom_input_query(9, 0); }
Vector2 BackendRaw_GetMousePosition(void)
{
    return (Vector2){(float)BackendRaw_GetMouseX(),
                     (float)BackendRaw_GetMouseY()};
}
Vector2 BackendRaw_GetMouseDelta(void)
{
    return (Vector2){(float)js_dom_input_float(0),
                     (float)js_dom_input_float(1)};
}
float BackendRaw_GetMouseWheelMove(void)
{
    return (float)js_dom_input_float(4);
}
Vector2 BackendRaw_GetMouseWheelMoveV(void)
{
    return (Vector2){(float)js_dom_input_float(2),
                     (float)js_dom_input_float(3)};
}
void SetMousePosition(int x, int y) { js_dom_input_set_mouse(x, y); }
void SetMouseOffset(int offsetX, int offsetY)
{
    js_dom_input_mouse_config(offsetX, offsetY, 1.0, 1.0);
}
void SetMouseScale(float scaleX, float scaleY)
{
    js_dom_input_mouse_config(0.0, 0.0, scaleX, scaleY);
}
int GetTouchX(void) { return js_dom_touch_query(0, 0); }
int GetTouchY(void) { return js_dom_touch_query(0, 1); }
Vector2 GetTouchPosition(int index)
{
    return (Vector2){(float)js_dom_touch_query(index, 0),
                     (float)js_dom_touch_query(index, 1)};
}
int GetTouchPointId(int index) { return js_dom_touch_query(index, 2); }
int GetTouchPointCount(void) { return js_dom_touch_query(0, 3); }
const char *GetKeyName(int key)
{
    static char name[16];
    if(key >= 32 && key < 127)
        snprintf(name, sizeof(name), "%c", key);
    else
        snprintf(name, sizeof(name), "%d", key);
    return name;
}

/* ------------------------------------------------------------------------- */
/* OS/misc                                                                   */
/* ------------------------------------------------------------------------- */

static char g_clipboard[4096];

const char *GetClipboardText(void)
{
    js_dom_clipboard_pull(g_clipboard, (int)sizeof(g_clipboard));
    return g_clipboard;
}
void SetClipboardText(const char *text)
{
    if(text == NULL)
        return;
    snprintf(g_clipboard, sizeof(g_clipboard), "%s", text);
    js_dom_clipboard_push(text);
}
bool FileExists(const char *fileName)
{
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 && !S_ISDIR(st.st_mode);
}
bool DirectoryExists(const char *dirPath)
{
    struct stat st;
    return dirPath != NULL && stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode);
}
static int dom_mkdir_recursive(const char *dirPath)
{
    char tmp[1024];
    size_t len;
    size_t i;

    if(dirPath == NULL || dirPath[0] == '\0')
        return -1;
    snprintf(tmp, sizeof(tmp), "%s", dirPath);
    len = strlen(tmp);
    while(len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for(i = 1; i < len; i++) {
        if(tmp[i] != '/')
            continue;
        tmp[i] = '\0';
        if(tmp[0] != '\0' && !DirectoryExists(tmp) && mkdir(tmp, 0777) != 0)
            return -1;
        tmp[i] = '/';
    }
    if(!DirectoryExists(tmp) && mkdir(tmp, 0777) != 0)
        return -1;
    return DirectoryExists(tmp) ? 0 : -1;
}
int MakeDirectory(const char *dirPath) { return dom_mkdir_recursive(dirPath); }
bool SaveFileData(const char *fileName, const void *data, int bytesToWrite)
{
    FILE *f;
    if(fileName == NULL || data == NULL || bytesToWrite < 0)
        return false;
    f = fopen(fileName, "wb");
    if(f == NULL)
        return false;
    if(fwrite(data, 1, (size_t)bytesToWrite, f) != (size_t)bytesToWrite) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}
bool SaveFileText(const char *fileName, const char *text)
{
    return text != NULL && SaveFileData(fileName, text, (int)strlen(text));
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
void UnloadFileData(unsigned char *data) { free(data); }
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
void UnloadFileText(char *text) { free(text); }
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
int ChangeDirectory(const char *dir)
{
    return dir != NULL && chdir(dir) == 0 ? 0 : -1;
}
const char *GetWorkingDirectory(void)
{
    static char cwd[1024];
    return getcwd(cwd, sizeof(cwd)) != NULL ? cwd : ".";
}
long GetFileModTime(const char *fileName)
{
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 ? (long)st.st_mtime : 0;
}
int GetFileLength(const char *fileName)
{
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 && !S_ISDIR(st.st_mode)
        ? (int)st.st_size : 0;
}
static const char *dom_basename(const char *path)
{
    const char *slash = path != NULL ? strrchr(path, '/') : NULL;
    return slash != NULL && slash[1] != '\0' ? slash + 1 : path;
}
const char *GetFileName(const char *filePath) { return dom_basename(filePath); }
const char *GetFileExtension(const char *fileName)
{
    const char *base = dom_basename(fileName);
    const char *dot = base != NULL ? strrchr(base, '.') : NULL;
    return dot != NULL && dot[1] != '\0' ? dot : "";
}
bool IsFileExtension(const char *fileName, const char *ext)
{
    return ext != NULL && strcmp(GetFileExtension(fileName), ext) == 0;
}
const char *GetFileNameWithoutExt(const char *filePath)
{
    static char name[1024];
    const char *base = dom_basename(filePath);
    const char *dot;
    int len;

    if(base == NULL)
        return "";
    dot = strrchr(base, '.');
    len = dot != NULL ? (int)(dot - base) : (int)strlen(base);
    if(len >= (int)sizeof(name))
        len = (int)sizeof(name) - 1;
    memcpy(name, base, (size_t)len);
    name[len] = '\0';
    return name;
}
const char *GetPrevDirectoryPath(const char *dirPath)
{
    static char prev[1024];
    char *slash;
    size_t len;

    if(dirPath == NULL || dirPath[0] == '\0')
        return ".";
    snprintf(prev, sizeof(prev), "%s", dirPath);
    len = strlen(prev);
    while(len > 1 && prev[len - 1] == '/')
        prev[--len] = '\0';
    slash = strrchr(prev, '/');
    if(slash == NULL)
        return ".";
    if(slash == prev) {
        prev[1] = '\0';
        return prev;
    }
    *slash = '\0';
    return prev;
}
bool IsPathFile(const char *path) { return FileExists(path); }
bool IsPathDirectory(const char *path) { return DirectoryExists(path); }
bool IsFileNameValid(const char *fileName)
{
    return fileName != NULL && fileName[0] != '\0';
}
static void dom_free_path_list(FilePathList files)
{
    unsigned int i;
    if(files.paths == NULL)
        return;
    for(i = 0; i < files.count; i++)
        free(files.paths[i]);
    free(files.paths);
}
FilePathList LoadDroppedFiles(void)
{
    FilePathList files = {0};
    int count = js_dom_dropped_count();
    int i;

    if(count <= 0)
        return files;
    files.paths = calloc((size_t)count, sizeof(char *));
    if(files.paths == NULL)
        return files;
    files.count = (unsigned int)count;
    for(i = 0; i < count; i++)
        files.paths[i] = js_dom_dropped_path(i);
    return files;
}
bool IsFileDropped(void) { return js_dom_dropped_count() > 0; }
void UnloadDroppedFiles(FilePathList files) { dom_free_path_list(files); }
FilePathList LoadDirectoryFiles(const char *dirPath)
{
    return LoadDirectoryFilesEx(dirPath, NULL, false);
}
FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter,
                                  bool scanSubdirs)
{
    FilePathList files = {0};
    DIR *dir;
    struct dirent *entry;
    char **paths = NULL;
    unsigned int count = 0;
    unsigned int capacity = 0;

    (void)scanSubdirs;
    dir = opendir(basePath != NULL ? basePath : ".");
    if(dir == NULL)
        return files;
    while((entry = readdir(dir)) != NULL) {
        char path[1024];
        char *copy;

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if(filter != NULL && filter[0] != '\0' &&
           strcmp(filter, "*.*") != 0 && !IsFileExtension(entry->d_name, filter))
            continue;
        snprintf(path, sizeof(path), "%s/%s", basePath != NULL ? basePath : ".",
                 entry->d_name);
        if(count == capacity) {
            unsigned int new_capacity = capacity == 0 ? 8 : capacity * 2;
            char **grown = realloc(paths, (size_t)new_capacity * sizeof(char *));
            if(grown == NULL)
                break;
            paths = grown;
            capacity = new_capacity;
        }
        copy = malloc(strlen(path) + 1);
        if(copy == NULL)
            break;
        strcpy(copy, path);
        paths[count++] = copy;
    }
    closedir(dir);
    files.count = count;
    files.paths = paths;
    return files;
}
void UnloadDirectoryFiles(FilePathList files) { dom_free_path_list(files); }
unsigned int GetDirectoryFileCount(const char *dirPath)
{
    FilePathList files = LoadDirectoryFiles(dirPath);
    unsigned int count = files.count;
    UnloadDirectoryFiles(files);
    return count;
}
unsigned int GetDirectoryFileCountEx(const char *basePath, const char *filter,
                                     bool scanSubdirs)
{
    FilePathList files = LoadDirectoryFilesEx(basePath, filter, scanSubdirs);
    unsigned int count = files.count;
    UnloadDirectoryFiles(files);
    return count;
}
void SetShapesTexture(Texture2D texture, Rectangle rec)
{
    (void)texture;
    (void)rec;
}
void SetTraceLogLevel(int logLevel) { (void)logLevel; }
void SetTraceLogCallback(TraceLogCallback callback) { (void)callback; }
const char *TextFormat(const char *text, ...)
{
    static char buffers[16][256];
    static int next;
    va_list args;
    int slot = next++ % 16;

    va_start(args, text);
    vsnprintf(buffers[slot], sizeof(buffers[slot]), text, args);
    va_end(args);
    return buffers[slot];
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

#endif /* __EMSCRIPTEN__ */
