// Raw browser effects; state and exported host policy live in canvas_window.zi.
addToLibrary({
  js_canvas_boot__deps: ["$UTF8ToString", "$FS"],
  js_canvas_boot__sig: "viii",
  js_canvas_boot: function(w, h, title) {
    var g = globalThis;
    if (g.__kryCanvas && g.__kryCanvas.close) g.__kryCanvas.close();
    var K = g.__kryCanvas = {
        w: w, h: h, renderW: w, renderH: h, dpi: 1,
        canvas: null, ctx: null,
        textures: {}, nextTex: 1,
        target: [],               /* render-target stack: {canvas,ctx} */
        saved: 0,                 /* active screen-space scissor save */
        mode2D: null,
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
    K.listeners = [];
    K.listen = function(target, name, callback, options) {
        if (!target || !target.addEventListener) return;
        target.addEventListener(name, callback, options);
        K.listeners.push([target, name, callback, options]);
    };
    K.close = function() {
        if (K.closed) return;
        K.closed = true;
        K.listeners.forEach(function(hook) { hook[0].removeEventListener(hook[1], hook[2], hook[3]); });
        K.listeners = [];
        if (K.ro) K.ro.disconnect();
        if (K.resizeTimer) clearTimeout(K.resizeTimer);
        K.textures = {}; K.tints = {}; K.target = [];
        K.ctx = null;
        if (K.createdCanvas && K.canvas && K.canvas.parentNode) K.canvas.parentNode.removeChild(K.canvas);
    };
    K.enqueue = function(queue, value) {
        if (queue.length < 1024) queue.push(value);
    };
    K.ctxNow = function () {
        return K.target.length ? K.target[K.target.length - 1].ctx
                               : K.ctx;
    };
    K.applyMode2D = function (ctx) {
        if (!K.mode2D) return;
        var m = K.mode2D;
        ctx.save();
        ctx.translate(m.offsetX, m.offsetY);
        ctx.rotate(m.rotation * Math.PI / 180.0);
        ctx.scale(m.zoomX, m.zoomY);
        ctx.translate(-m.targetX, -m.targetY);
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
    K.layoutBox = function () {
        var doc = (typeof document !== 'undefined') ? document : null;
        var frame = doc ? doc.getElementById('canvas-frame') : null;
        var parent = K.canvas && K.canvas.parentElement ? K.canvas.parentElement : null;
        return frame || parent || K.canvas || null;
    };
    K.measureLayout = function (fallbackW, fallbackH) {
        var w = fallbackW;
        var h = fallbackH;
        var box = K.layoutBox();
        if (box && box.getBoundingClientRect) {
            var r = box.getBoundingClientRect();
            if (r.width > 0 && r.height > 0) {
                w = r.width;
                h = r.height;
            }
        }
        return {
            w: Math.max(1, Math.round(w || fallbackW || K.w || 1)),
            h: Math.max(1, Math.round(h || fallbackH || K.h || 1)),
            fluid: box && box !== K.canvas
        };
    };
    K.resizeMainCanvas = function (nw, nh) {
        if (K.closed) return;
        var dpr = Math.max(1, g.devicePixelRatio || 1);
        var c = K.canvas;
        var layout = K.measureLayout(nw, nh);
        nw = layout.w;
        nh = layout.h;
        if (c && c.style) {
            if (layout.fluid) {
                c.style.width = '100%';
                c.style.height = '100%';
            } else {
                c.style.width = Math.max(1, nw | 0) + 'px';
                c.style.height = Math.max(1, nh | 0) + 'px';
            }
            c.style.touchAction = 'none';
        }
        if (!layout.fluid && c && c.clientWidth > 0 && c.clientHeight > 0) {
            nw = c.clientWidth;
            nh = c.clientHeight;
        }
        nw = Math.max(1, nw | 0);
        nh = Math.max(1, nh | 0);
        if (K.ctx && dpr === K.dpi && nw === K.w && nh === K.h &&
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
            if (!K.ctx || c.width !== K.renderW || c.height !== K.renderH) {
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
            K.canvas.id = 'canvas';
            K.createdCanvas = true;
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
        MetaLeft: 343, MetaRight: 347, Semicolon: 59, Equal: 61, Comma: 44,
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
    var claimEvent = function (e, name) {
        var key = '__kryCanvasHandled_' + name;
        if (!e || e[key]) return false;
        try { e[key] = 1; } catch (_) {}
        return true;
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
            K.enqueue(K.buttonsPressed, 0);
        } else if (hadTouches && K.touches.length === 0) {
            delete K.buttonsDown[0];
            K.enqueue(K.buttonsReleased, 0);
        }
    };
    var hook = function (target) {
        if (!target || !target.addEventListener)
            return;
        K.listen(target, 'mousemove', function (e) {
            if (!claimEvent(e, 'mousemove')) return;
            var p = localPoint(e);
            setMouse(p.x, p.y);
        });
        K.listen(target, 'mousedown', function (e) {
            if (!claimEvent(e, 'mousedown')) return;
            var p = localPoint(e);
            setMouse(p.x, p.y);
            if (K.canvas && K.canvas.focus) {
                try { K.canvas.focus({preventScroll: true}); } catch (_) {
                    try { K.canvas.focus(); } catch (_) {}
                }
            }
            K.buttonsDown[e.button] = 1;
            K.enqueue(K.buttonsPressed, e.button);
            if (K.canvas && K.canvas.setPointerCapture &&
                e.pointerId !== undefined) {
                try { K.canvas.setPointerCapture(e.pointerId); } catch (_) {}
            }
        });
        K.listen(target, 'mouseup', function (e) {
            if (!claimEvent(e, 'mouseup')) return;
            var p = localPoint(e);
            setMouse(p.x, p.y);
            delete K.buttonsDown[e.button];
            K.enqueue(K.buttonsReleased, e.button);
        });
        K.listen(target, 'wheel', function (e) {
            if (!claimEvent(e, 'wheel')) return;
            var unit = e.deltaMode === 1 ? 16 : (e.deltaMode === 2 ? K.h : 120);
            K.wheelX += -e.deltaX / unit;
            K.wheelY += -e.deltaY / unit;
            if (e.cancelable) e.preventDefault();
        });
        K.listen(target, 'keydown', function (e) {
            if (!claimEvent(e, 'keydown')) return;
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                if (K.keysDown[k] && e.repeat) K.enqueue(K.keysRepeated, k);
                else if (!K.keysDown[k]) K.enqueue(K.keysPressed, k);
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
        K.listen(target, 'keyup', function (e) {
            if (!claimEvent(e, 'keyup')) return;
            var k = keyOf(e.code || "", e.key || "");
            if (k) {
                delete K.keysDown[k];
                K.enqueue(K.keysReleased, k);
                if (k === 32 || (k >= 256 && k <= 269))
                    e.preventDefault();
            }
        });
        K.listen(target, 'keypress', function (e) {
            if (!claimEvent(e, 'keypress')) return;
            if (e.key && Array.from(e.key).length === 1)
                K.enqueue(K.chars, e.key.codePointAt(0));
        });
        K.listen(target, 'touchstart', function (e) {
            if (!claimEvent(e, 'touchstart')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        K.listen(target, 'touchmove', function (e) {
            if (!claimEvent(e, 'touchmove')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        K.listen(target, 'touchend', function (e) {
            if (!claimEvent(e, 'touchend')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
        K.listen(target, 'touchcancel', function (e) {
            if (!claimEvent(e, 'touchcancel')) return;
            syncTouches(e.touches || []);
            if (e.cancelable) e.preventDefault();
        }, {passive: false});
    };
    var hookDrop = function (target) {
        if (!target || !target.addEventListener)
            return;
        K.listen(target, 'dragover', function (e) {
            if (e.preventDefault) e.preventDefault();
        });
        K.listen(target, 'drop', function (e) {
            if (!claimEvent(e, 'drop')) return;
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
        K.listen(g, 'focus', function () { K.focused = 1; });
        K.listen(g, 'blur', function () { K.focused = 0; K.keysDown = {}; K.buttonsDown = {}; K.touches = []; });
        K.listen(g, 'paste', function (e) {
            var data = e.clipboardData || (g.clipboardData || null);
            var text = data && data.getData ? data.getData('text') : "";
            if (text) K.clipboard = text;
        });
        K.listen(g, 'copy', function (e) {
            if (!K.clipboard) return;
            var data = e.clipboardData || (g.clipboardData || null);
            if (data && data.setData) {
                data.setData('text/plain', K.clipboard);
                if (e.preventDefault) e.preventDefault();
            }
        });
        K.listen(g, 'cut', function (e) {
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
        try { K.ro.observe(K.layoutBox()); } catch (_) {}
    }
    if (g.addEventListener) {
        var scheduleResize = function () {
            if (K.resizeTimer) return;
            K.resizeTimer = setTimeout(function () {
                K.resizeTimer = 0;
                K.resizeMainCanvas(K.w, K.h);
            }, 0);
        };
        K.listen(g, 'resize', scheduleResize);
        if (g.visualViewport)
            K.listen(g.visualViewport, 'resize', scheduleResize);
    }
  },
  js_canvas_resize__sig: "vii",
  js_canvas_resize: function(w, h) {
    var K = globalThis.__kryCanvas;
    if (K && K.resizeMainCanvas) K.resizeMainCanvas(w, h);
  },
  js_canvas_dim__sig: "ii",
  js_canvas_dim: function(which) {
    var K = globalThis.__kryCanvas;
    if (!K) return 0;
    switch (which) {
    case 0: return K.w | 0;
    case 1: return K.h | 0;
    case 2: return K.renderW | 0;
    case 3: return K.renderH | 0;
    case 4: { var r = K.resized ? 1 : 0; K.resized = 0; return r; }
    case 5: return K.focused ? 1 : 0;
    case 6: return K.closed ? 1 : 0;
    }
    return 0;
  },
  js_canvas_dpi__sig: "d",
  js_canvas_dpi: function() {
    var K = globalThis.__kryCanvas;
    return K ? K.dpi : 1.0;
  },
  js_canvas_set_cursor__sig: "vi",
  js_canvas_set_cursor: function(cursor) {
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
  },
  js_canvas_set_title__deps: ["$UTF8ToString"],
  js_canvas_set_title__sig: "vi",
  js_canvas_set_title: function(title) {
    if (typeof document !== 'undefined' && document.title !== undefined)
        document.title = title ? UTF8ToString(title) : "";
  },
  js_canvas_focus__sig: "v",
  js_canvas_focus: function() {
    var K = globalThis.__kryCanvas;
    if (K && K.canvas && K.canvas.focus) K.canvas.focus();
  },
  js_canvas_fullscreen__sig: "vi",
  js_canvas_fullscreen: function(enable) {
    var K = globalThis.__kryCanvas;
    if (typeof document === 'undefined' || !K || !K.canvas) return;
    if (enable) {
        if (!document.fullscreenElement && K.canvas.requestFullscreen)
            K.canvas.requestFullscreen().catch(function () {});
    } else {
        if (document.fullscreenElement === K.canvas && document.exitFullscreen)
            document.exitFullscreen().catch(function () {});
    }
  },
  js_canvas_fullscreen_state__sig: "i",
  js_canvas_fullscreen_state: function() {
    if (typeof document === 'undefined') return 0;
    var K = globalThis.__kryCanvas;
    return K && document.fullscreenElement === K.canvas ? 1 : 0;
  },
  js_ctx_call__sig: "vidddddddiiii",
  js_ctx_call: function(op, a, b, c, d, e, f, g2, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    if (!K || K.closed) return;
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
    case 1: { /* fill rect: share exact device-pixel edges after camera zoom */
            ctx.fillStyle = col;
            var transform = ctx.getTransform();
            if (transform.b === 0 && transform.c === 0 &&
                transform.a > 0 && transform.d > 0) {
                var x0 = Math.round(transform.a * a + transform.e);
                var y0 = Math.round(transform.d * b + transform.f);
                var x1 = Math.round(transform.a * (a + c) + transform.e);
                var y1 = Math.round(transform.d * (b + d) + transform.f);
                ctx.fillRect((x0 - transform.e) / transform.a,
                             (y0 - transform.f) / transform.d,
                             (x1 - x0) / transform.a,
                             (y1 - y0) / transform.d);
            } else {
                ctx.fillRect(a, b, c, d);
            }
            break; }
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
    case 9: /* scissor begin: replace the screen-space clip below the camera */
             if (K.mode2D) ctx.restore();
             if (K.saved > 0) { ctx.restore(); K.saved = 0; }
             ctx.save();
             ctx.setTransform(K.target.length ? 1 : K.dpi, 0, 0,
                              K.target.length ? 1 : K.dpi, 0, 0);
             ctx.beginPath(); ctx.rect(a, b, c, d); ctx.clip();
             K.saved = 1;
             K.applyMode2D(ctx);
             break;
    case 10: /* scissor end */
             if (K.saved > 0) {
                 if (K.mode2D) ctx.restore();
                 ctx.restore(); K.saved = 0;
                 K.applyMode2D(ctx);
             }
             break;
    case 11: /* mode2d push: raylib camera transform */
             K.mode2D = { zoomX: a, zoomY: b, offsetX: c, offsetY: d,
                          targetX: e, targetY: f, rotation: g2 };
             K.applyMode2D(ctx);
             break;
    case 12: /* mode2d pop */
             if (K.mode2D) { ctx.restore(); K.mode2D = null; }
             break;
    case 13: /* frame reset: logical screen coords over HiDPI backing */
             if (K.mode2D) { ctx.restore(); K.mode2D = null; }
             if (K.saved > 0) { ctx.restore(); K.saved = 0; }
             ctx.setTransform(K.target.length ? 1 : K.dpi, 0, 0,
                              K.target.length ? 1 : K.dpi, 0, 0);
             break;
    }
  },
  js_canvas_wait_frame__async: true,
  js_canvas_wait_frame__deps: ["$Asyncify"],
  js_canvas_wait_frame__sig: "vdi",
  js_canvas_wait_frame: function(min_delay_ms, target_fps) {
    return Asyncify.handleAsync(async function() {
    var K = globalThis.__kryCanvas;
    if (K) {
        K.targetFps = target_fps | 0;
        K.lastFrameDelayMs = min_delay_ms;
    }

    if (typeof requestAnimationFrame === 'function') {
        var earliest = (typeof performance !== 'undefined' && performance.now)
            ? performance.now() + Math.max(0, min_delay_ms)
            : 0;
        await new Promise(function (resolve) {
            function step(t) {
                if (t + 0.25 >= earliest) resolve();
                else requestAnimationFrame(step);
            }
            requestAnimationFrame(step);
        });
        return;
    }

    await new Promise(function (resolve) {
        setTimeout(resolve, Math.max(1, min_delay_ms | 0));
    });
    });
  },
  js_canvas_now__sig: 'd',
  js_canvas_now: function() { return performance.now(); },
  js_canvas_close__sig: 'v',
  js_canvas_close: function() { var K = globalThis.__kryCanvas; if (K && K.close) K.close(); },
  js_canvas_frame__sig: 'v',
  js_canvas_frame: function() { var K = globalThis.__kryCanvas; if (K) K.frames++; },
  js_canvas_sleep__async: true,
  js_canvas_sleep__deps: ['$Asyncify'],
  js_canvas_sleep__sig: 'vi',
  js_canvas_sleep: function(milliseconds) { return Asyncify.handleAsync(async function() { await new Promise(function(resolve) { setTimeout(resolve, Math.max(0, milliseconds)); }); }); },
  js_canvas_style__sig: 'vid',
  js_canvas_style: function(property, value) {
    var K = globalThis.__kryCanvas, style = K && K.canvas && K.canvas.style;
    if (!style) return;
    if (property === 0) style.opacity = Math.max(0, Math.min(1, value));
    else { var names = ['', 'minWidth', 'minHeight', 'maxWidth', 'maxHeight'];
      if (names[property]) style[names[property]] = Math.max(0, value) + 'px'; }
  },
  js_canvas_raster_clip_push__sig: 'viiii',
  js_canvas_raster_clip_push: function(x, y, width, height) {
    var K = globalThis.__kryCanvas, ctx = K && K.ctxNow();
    if (!ctx) return;
    ctx.save();
    var transform = ctx.getTransform();
    var dpi = K.target.length ? 1 : K.dpi;
    ctx.setTransform(dpi, 0, 0, dpi, 0, 0);
    ctx.beginPath(); ctx.rect(x, y, Math.max(0, width), Math.max(0, height)); ctx.clip();
    ctx.setTransform(transform);
    K.rasterClips = (K.rasterClips || 0) + 1;
  },
  js_canvas_raster_clip_pop__sig: 'v',
  js_canvas_raster_clip_pop: function() {
    var K = globalThis.__kryCanvas, ctx = K && K.ctxNow();
    if (ctx && K.rasterClips > 0) { ctx.restore(); K.rasterClips--; }
  },
});
