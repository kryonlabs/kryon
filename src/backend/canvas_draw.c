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

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
