// Raw browser effects; state and exported host policy live in canvas_draw.zi.
addToLibrary({
  js_draw_gradient_v__sig: "vddddiiiiiiii",
  js_draw_gradient_v: function(x, y, w, h, tr, tg, tb, ta, br, bg, bb, ba) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var gr = ctx.createLinearGradient(0, y, 0, y + h);
    gr.addColorStop(0, K.col(tr, tg, tb, ta));
    gr.addColorStop(1, K.col(br, bg, bb, ba));
    ctx.fillStyle = gr;
    ctx.fillRect(x, y, w, h);
  },
  js_draw_gradient_h__sig: "vddddiiiiiiii",
  js_draw_gradient_h: function(x, y, w, h, lr, lg, lb, la, rr, rg, rb, ra) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var gr = ctx.createLinearGradient(x, 0, x + w, 0);
    gr.addColorStop(0, K.col(lr, lg, lb, la));
    gr.addColorStop(1, K.col(rr, rg, rb, ra));
    ctx.fillStyle = gr;
    ctx.fillRect(x, y, w, h);
  },
  js_draw_circle_gradient__sig: "vdddiiiiiiii",
  js_draw_circle_gradient: function(x, y, radius, ir, ig, ib, ia, or_, og, ob, oa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx || radius <= 0) return;
    var gr = ctx.createRadialGradient(x, y, 0, x, y, radius);
    gr.addColorStop(0, K.col(ir, ig, ib, ia));
    gr.addColorStop(1, K.col(or_, og, ob, oa));
    ctx.fillStyle = gr;
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2, false);
    ctx.fill();
  },
  js_blend_mode__sig: "vii",
  js_blend_mode: function(mode, begin) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    if (!begin) { ctx.restore(); return; }
    ctx.save();
    if (mode === 1) ctx.globalCompositeOperation = 'lighter';
    else if (mode === 2) ctx.globalCompositeOperation = 'multiply';
    else ctx.globalCompositeOperation = 'source-over';
  },
  js_draw_gradient_ex__sig: "vddddiiiiiiiiiiiiiiii",
  js_draw_gradient_ex: function(x, y, w, h, c1r, c1g, c1b, c1a, c2r, c2g, c2b, c2a, c3r, c3g, c3b, c3a, c4r, c4g, c4b, c4a) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx || w <= 0 || h <= 0) return;
    var iw = Math.max(1, Math.ceil(w));
    var ih = Math.max(1, Math.ceil(h));
    var cv = K.makeCanvas(iw, ih);
    if (!cv) return;
    var c2d = cv.getContext('2d');
    var img = c2d.createImageData(iw, ih);
    var data = img.data;
    for (var py = 0; py < ih; py++) {
        var ty = ih <= 1 ? 0 : py / (ih - 1);
        for (var px = 0; px < iw; px++) {
            var tx = iw <= 1 ? 0 : px / (iw - 1);
            var i = (py * iw + px) * 4;
            var topR = c1r + (c2r - c1r) * tx;
            var topG = c1g + (c2g - c1g) * tx;
            var topB = c1b + (c2b - c1b) * tx;
            var topA = c1a + (c2a - c1a) * tx;
            var botR = c4r + (c3r - c4r) * tx;
            var botG = c4g + (c3g - c4g) * tx;
            var botB = c4b + (c3b - c4b) * tx;
            var botA = c4a + (c3a - c4a) * tx;
            data[i + 0] = topR + (botR - topR) * ty;
            data[i + 1] = topG + (botG - topG) * ty;
            data[i + 2] = topB + (botB - topB) * ty;
            data[i + 3] = topA + (botA - topA) * ty;
        }
    }
    c2d.putImageData(img, 0, 0);
    ctx.drawImage(cv, x, y, w, h);
  },
  js_draw_rounded__sig: "vidddddiiii",
  js_draw_rounded: function(op, x, y, w, h, rad, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var col = K.col(r, gg, bb, aa);
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
  },
  js_draw_rect_lines_ex__sig: "vdddddiiii",
  js_draw_rect_lines_ex: function(x, y, w, h, thick, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx || w <= 0 || h <= 0) return;
    thick = Math.max(1.0, thick);
    ctx.save();
    ctx.strokeStyle = K.col(r, gg, bb, aa);
    ctx.lineWidth = thick;
    ctx.strokeRect(x + thick * 0.5, y + thick * 0.5,
                   Math.max(0, w - thick), Math.max(0, h - thick));
    ctx.restore();
  },
  js_draw_rect_pro__sig: "vdddddddiiii",
  js_draw_rect_pro: function(x, y, w, h, ox, oy, rot, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    ctx.save();
    ctx.fillStyle = K.col(r, gg, bb, aa);
    ctx.translate(x, y);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    ctx.fillRect(-ox, -oy, w, h);
    ctx.restore();
  },
  js_draw_circle_lines_ex__sig: "vddddiiii",
  js_draw_circle_lines_ex: function(cx, cy, radius, thick, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx || radius <= 0) return;
    ctx.save();
    ctx.strokeStyle = K.col(r, gg, bb, aa);
    ctx.lineWidth = Math.max(1.0, thick);
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2, false);
    ctx.stroke();
    ctx.restore();
  },
  js_draw_ring__sig: "vddddddiiii",
  js_draw_ring: function(cx, cy, inner, outer, start, end, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var s0 = start * Math.PI / 180.0;
    var s1 = end * Math.PI / 180.0;
    var full = Math.abs(end - start) >= 359.999;
    ctx.fillStyle = K.col(r, gg, bb, aa);
    ctx.beginPath();
    if (full) {
        ctx.arc(cx, cy, outer, 0, Math.PI * 2, false);
        ctx.moveTo(cx + inner, cy);
        ctx.arc(cx, cy, inner, Math.PI * 2, 0, true);
    } else {
        ctx.arc(cx, cy, outer, s0, s1, false);
        ctx.arc(cx, cy, inner, s1, s0, true);
        ctx.closePath();
    }
    ctx.fill('evenodd');
  },
  js_draw_ring_lines__sig: "vddddddiiii",
  js_draw_ring_lines: function(cx, cy, inner, outer, start, end, r, gg, bb, aa) {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var s0 = start * Math.PI / 180.0;
    var s1 = end * Math.PI / 180.0;
    var full = Math.abs(end - start) >= 359.999;
    ctx.save();
    ctx.strokeStyle = K.col(r, gg, bb, aa);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, outer, s0, s1, false);
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(cx, cy, inner, s0, s1, false);
    ctx.stroke();
    if (!full) {
        ctx.beginPath();
        ctx.moveTo(cx + Math.cos(s0) * inner, cy + Math.sin(s0) * inner);
        ctx.lineTo(cx + Math.cos(s0) * outer, cy + Math.sin(s0) * outer);
        ctx.moveTo(cx + Math.cos(s1) * inner, cy + Math.sin(s1) * inner);
        ctx.lineTo(cx + Math.cos(s1) * outer, cy + Math.sin(s1) * outer);
        ctx.stroke();
    }
    ctx.restore();
  },
  js_get_modelview__sig: "vi",
  js_get_modelview: function(values) {
    var K = globalThis.__kryCanvas;
    var ctx = K && K.ctxNow();
    if (!ctx) return;
    var transform = ctx.getTransform();
    var scale = K.target.length ? 1 : K.dpi;
    HEAPF32[values / 4] = transform.a / scale;
    HEAPF32[values / 4 + 1] = transform.b / scale;
    HEAPF32[values / 4 + 2] = transform.c / scale;
    HEAPF32[values / 4 + 3] = transform.d / scale;
    HEAPF32[values / 4 + 4] = transform.e / scale;
    HEAPF32[values / 4 + 5] = transform.f / scale;
  },
  js_set_modelview__sig: "vdddddd",
  js_set_modelview: function(a, b, c, d, e, f) {
    var K = globalThis.__kryCanvas;
    var ctx = K && K.ctxNow();
    if (!ctx) return;
    var scale = K.target.length ? 1 : K.dpi;
    ctx.setTransform(a * scale, b * scale, c * scale, d * scale,
                     e * scale, f * scale);
  },
});
