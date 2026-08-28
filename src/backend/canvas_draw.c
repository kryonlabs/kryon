/*
 * canvas_draw.c — shape drawing over the 2d context.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * Simple shapes go through the js_ctx_call opcode dispatcher in
 * canvas_window.c; gradients, rounded rects, and rings have dedicated
 * EM_JS here because they need extra arguments.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/* Drawing                                                            */
/* ------------------------------------------------------------------ */

/* Gradients need both full colors; rounded rects need a radius. */
EM_JS(void, js_draw_gradient_v, (double x, double y, double w, double h,
                                 int tr, int tg, int tb, int ta,
                                 int br, int bg, int bb, int ba), {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var gr = ctx.createLinearGradient(0, y, 0, y + h);
    gr.addColorStop(0, K.col(tr, tg, tb, ta));
    gr.addColorStop(1, K.col(br, bg, bb, ba));
    ctx.fillStyle = gr;
    ctx.fillRect(x, y, w, h);
});

EM_JS(void, js_draw_gradient_h, (double x, double y, double w, double h,
                                 int lr, int lg, int lb, int la,
                                 int rr, int rg, int rb, int ra), {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    var gr = ctx.createLinearGradient(x, 0, x + w, 0);
    gr.addColorStop(0, K.col(lr, lg, lb, la));
    gr.addColorStop(1, K.col(rr, rg, rb, ra));
    ctx.fillStyle = gr;
    ctx.fillRect(x, y, w, h);
});

EM_JS(void, js_draw_gradient_ex, (double x, double y, double w, double h,
                                  int c1r, int c1g, int c1b, int c1a,
                                  int c2r, int c2g, int c2b, int c2a,
                                  int c3r, int c3g, int c3b, int c3a,
                                  int c4r, int c4g, int c4b, int c4a), {
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
});

EM_JS(void, js_draw_rounded, (int op, double x, double y, double w,
                              double h, double rad,
                              int r, int gg, int bb, int aa), {
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
});

EM_JS(void, js_draw_rect_lines_ex, (double x, double y, double w, double h,
                                    double thick,
                                    int r, int gg, int bb, int aa), {
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
});

EM_JS(void, js_draw_rect_pro, (double x, double y, double w, double h,
                               double ox, double oy, double rot,
                               int r, int gg, int bb, int aa), {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    ctx.save();
    ctx.fillStyle = K.col(r, gg, bb, aa);
    ctx.translate(x, y);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    ctx.fillRect(-ox, -oy, w, h);
    ctx.restore();
});

EM_JS(void, js_draw_circle_lines_ex, (double cx, double cy, double radius,
                                      double thick,
                                      int r, int gg, int bb, int aa), {
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
});

/* Ring as a true filled annulus segment: outer arc forward, inner arc
 * back, even-odd fill. */
EM_JS(void, js_draw_ring, (double cx, double cy, double inner, double outer,
                           double start, double end,
                           int r, int gg, int bb, int aa), {
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
});

EM_JS(void, js_draw_ring_lines, (double cx, double cy, double inner,
                                 double outer, double start, double end,
                                 int r, int gg, int bb, int aa), {
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
});

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

void DrawPixel(int posX, int posY, Color color)
{
    DrawRectangle(posX, posY, 1, 1, color);
}

void DrawPixelV(Vector2 position, Color color)
{
    DrawPixel((int)position.x, (int)position.y, color);
}

void DrawLineV(Vector2 startPos, Vector2 endPos, Color color)
{
    DrawLine((int)startPos.x, (int)startPos.y, (int)endPos.x, (int)endPos.y,
             color);
}

void DrawLineStrip(const Vector2 *points, int pointCount, Color color)
{
    int i;

    if(points == NULL || pointCount < 2)
        return;
    for(i = 1; i < pointCount; i++)
        DrawLineV(points[i - 1], points[i], color);
}

void DrawRectangleRec(Rectangle rec, Color color)
{
    js_ctx_call(1, rec.x, rec.y, rec.width, rec.height, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangleV(Vector2 position, Vector2 size, Color color)
{
    DrawRectangleRec((Rectangle){position.x, position.y, size.x, size.y},
                     color);
}

void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation,
                      Color color)
{
    js_draw_rect_pro(rec.x, rec.y, rec.width, rec.height, origin.x, origin.y,
                     rotation, color.r, color.g, color.b, color.a);
}

void DrawRectangleLines(int posX, int posY, int width, int height,
                        Color color)
{
    js_ctx_call(2, posX, posY, width, height, 0, 0, 0,
                color.r, color.g, color.b, color.a);
}

void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    js_draw_rect_lines_ex(rec.x, rec.y, rec.width, rec.height, lineThick,
                          color.r, color.g, color.b, color.a);
}

void DrawRectangleGradientV(int posX, int posY, int width, int height,
                            Color top, Color bottom)
{
    js_draw_gradient_v(posX, posY, width, height,
                       top.r, top.g, top.b, top.a,
                       bottom.r, bottom.g, bottom.b, bottom.a);
}

void DrawRectangleGradientH(int posX, int posY, int width, int height,
                            Color left, Color right)
{
    js_draw_gradient_h(posX, posY, width, height,
                       left.r, left.g, left.b, left.a,
                       right.r, right.g, right.b, right.a);
}

void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2,
                             Color col3, Color col4)
{
    js_draw_gradient_ex(rec.x, rec.y, rec.width, rec.height,
                        col1.r, col1.g, col1.b, col1.a,
                        col2.r, col2.g, col2.b, col2.a,
                        col3.r, col3.g, col3.b, col3.a,
                        col4.r, col4.g, col4.b, col4.a);
}

static float canvas_round_radius(Rectangle rec, float roundness)
{
    return roundness * 0.5f * (rec.width < rec.height ? rec.width
                                                      : rec.height);
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                          Color color)
{
    (void)segments;
    js_draw_rounded(0, rec.x, rec.y, rec.width, rec.height,
                    canvas_round_radius(rec, roundness),
                    color.r, color.g, color.b, color.a);
}

void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                               Color color)
{
    (void)segments;
    js_draw_rounded(1, rec.x, rec.y, rec.width, rec.height,
                    canvas_round_radius(rec, roundness),
                    color.r, color.g, color.b, color.a);
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness,
                                 int segments, float lineThick, Color color)
{
    (void)segments;
    js_draw_rounded(1, rec.x - lineThick, rec.y - lineThick,
                    rec.width + lineThick * 2, rec.height + lineThick * 2,
                    canvas_round_radius(rec, roundness),
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

void DrawCircleLinesV(Vector2 center, float radius, Color color)
{
    DrawCircleLines((int)center.x, (int)center.y, radius, color);
}

void DrawCircleLinesEx(Vector2 center, float radius, float thick, Color color)
{
    js_draw_circle_lines_ex(center.x, center.y, radius, thick,
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

void DrawRingLines(Vector2 center, float innerRadius, float outerRadius,
                   float startAngle, float endAngle, int segments,
                   Color color)
{
    (void)segments;
    js_draw_ring_lines(center.x, center.y, innerRadius, outerRadius,
                       startAngle, endAngle,
                       color.r, color.g, color.b, color.a);
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    js_ctx_call(7, v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, 0,
                color.r, color.g, color.b, color.a);
}

void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    DrawLineV(v1, v2, color);
    DrawLineV(v2, v3, color);
    DrawLineV(v3, v1, color);
}

void DrawTriangleFan(const Vector2 *points, int pointCount, Color color)
{
    int i;

    if(points == NULL || pointCount < 3)
        return;
    for(i = 1; i < pointCount - 1; i++)
        DrawTriangle(points[0], points[i], points[i + 1], color);
}

void DrawTriangleStrip(const Vector2 *points, int pointCount, Color color)
{
    int i;

    if(points == NULL || pointCount < 3)
        return;
    for(i = 0; i < pointCount - 2; i++) {
        if((i & 1) == 0)
            DrawTriangle(points[i], points[i + 1], points[i + 2], color);
        else
            DrawTriangle(points[i + 1], points[i], points[i + 2], color);
    }
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

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
