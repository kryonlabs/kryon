#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/guide.h"
#include <stdio.h>

static GuideOverlayDebug g_guide_debug;

static void
guide_draw_scrim(GuideScrim scrim, Color color)
{
    DrawRectangleRec(scrim.top, color);
    DrawRectangleRec(scrim.bottom, color);
    DrawRectangleRec(scrim.left, color);
    DrawRectangleRec(scrim.right, color);
}

static void
guide_draw_arrow(Rectangle tip, Rectangle anchor)
{
    int anchor_cx = (int)(anchor.x + anchor.width / 2);
    int anchor_cy = (int)(anchor.y + anchor.height / 2);
    int tip_left = (int)tip.x;
    int tip_right = (int)(tip.x + tip.width);
    int tip_top = (int)tip.y;
    int tip_bottom = (int)(tip.y + tip.height);
    int arrow_size = Scale(10);
    Vector2 start, end;
    Vector2 tip0, tip1, tip2;
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    Color color = text_style.foreground;

    if(anchor_cy < tip_top) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy + anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_top;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x;
        tip0.y = end.y;
        tip1.x = end.x - arrow_size;
        tip1.y = end.y - arrow_size;
        tip2.x = end.x + arrow_size;
        tip2.y = end.y - arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else if(anchor_cy > tip_bottom) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy - anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_bottom;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x;
        tip0.y = end.y;
        tip1.x = end.x + arrow_size;
        tip1.y = end.y + arrow_size;
        tip2.x = end.x - arrow_size;
        tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else if(anchor_cx < tip_left) {
        start.x = (float)(anchor_cx + anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_left;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x;
        tip0.y = end.y;
        tip1.x = end.x - arrow_size;
        tip1.y = end.y - arrow_size;
        tip2.x = end.x - arrow_size;
        tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else {
        start.x = (float)(anchor_cx - anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_right;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x;
        tip0.y = end.y;
        tip1.x = end.x + arrow_size;
        tip1.y = end.y - arrow_size;
        tip2.x = end.x + arrow_size;
        tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    }
}

GuideResult
RenderGuideOverlay(GuideOverlayProps guide)
{
    GuideResult result = {0};
    GuideMetrics metrics = GuideMetricsFor((float)GetScale());
    GuideLayout layout;
    GuidePolicy policy;
    int view_w = guide.view_width > 0 ? guide.view_width : ui_view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : ui_view_height;
    int step;
    int tip_w;
    int line_gap = guide.line_gap > 0 ? guide.line_gap : metrics.default_line_gap;
    int max_tip_h;
    char page_text[32];
    ParagraphSpec paragraph;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    Color scrim;
    IconActionSpec icon_props;
    int previous_requested = 0;
    int next_requested = 0;
    int close_requested = 0;
    Style surface_style = ui_surface_style();
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());

    g_guide_debug.valid = 0;
    if(guide.steps == NULL || guide.count <= 0 || guide.step == NULL)
        return result;

    step = GuideStepFor(*guide.step, guide.count);
    *guide.step = step;
    result.step = step;

    next_requested = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER);
    previous_requested = IsKeyPressed(KEY_LEFT);
    close_requested = IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE);
    policy = GuidePolicyFor(step, guide.count, previous_requested != 0,
                            next_requested != 0, close_requested != 0);
    if(policy.closed || policy.finished || policy.changed) {
        *guide.step = policy.step;
        result.step = policy.step;
        result.changed = policy.changed;
        result.closed = policy.closed;
        result.finished = policy.finished;
        return result;
    }

    tip_w = GuideTipWidth(view_w, guide.max_width, metrics);
    max_tip_h = GuideMaxTipHeight(view_h, guide.reserved_top,
                                  guide.reserved_bottom, metrics);

    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = guide.steps[step].text;
    paragraph.width = tip_w - metrics.pad * 2;
    paragraph.font = guide.paragraph_font > 0 ? guide.paragraph_font : Text16;
    paragraph.line_gap = line_gap;
    paragraph_h = ui_paragraph_height(paragraph);
    while(paragraph.font > Text12 &&
          paragraph_h > max_tip_h - GuideChromeHeight(metrics)) {
        paragraph.font--;
        paragraph_h = ui_paragraph_height(paragraph);
    }

    tip_h = GuideTipHeight(paragraph_h, max_tip_h, metrics);
    tip = GuideTipBounds(guide.steps[step].anchor, tip_w, tip_h, view_w,
                         view_h, guide.reserved_top, guide.reserved_bottom,
                         metrics);
    layout = GuideLayoutFor(tip, paragraph.width, paragraph_h, step,
                            guide.count, metrics);
    SetModalCapture(tip);

    scrim.r = 0;
    scrim.g = 0;
    scrim.b = 0;
    scrim.a = 86;
    guide_draw_scrim(GuideScrimFor(view_w, view_h, guide.steps[step].anchor,
                                   metrics), scrim);
    DrawRectangleLinesEx(guide.steps[step].anchor,
                         (float)metrics.anchor_stroke,
                         text_style.foreground);
    guide_draw_arrow(tip, guide.steps[step].anchor);

    ui_draw_material(tip, (Rectangle){0}, surface_style.background,
                     surface_style.border, surface_style.border,
                     surface_style.radius, surface_style.border_width,
                     0.0f, 0.0f, 0, surface_style.focus, 0.0f,
                     surface_style.opacity, ui_style_fill(surface_style),
                     surface_style.material);

    memset(&icon_props, 0, sizeof(icon_props));
    icon_props.bounds = layout.close_button;
    icon_props.icon = guide.close_icon;
    icon_props.icon_size = metrics.close_icon_size;
    icon_props.icon_padding = metrics.close_icon_padding;
    if(RenderIconAction(icon_props)) {
        result.closed = 1;
        return result;
    }

    y = (int)layout.text.y;
    g_guide_debug.valid = 1;
    g_guide_debug.step = step;
    g_guide_debug.count = guide.count;
    g_guide_debug.paragraph_height = paragraph_h;
    g_guide_debug.text_clip_height = layout.text_clip_height;
    g_guide_debug.text_clipped = layout.text_clipped;
    g_guide_debug.tip = tip;
    g_guide_debug.text = layout.text;
    g_guide_debug.close_button = layout.close_button;
    g_guide_debug.back_button = (Rectangle){0};
    if(layout.text_clip_height > 0) {
        if(layout.text_clipped)
            BeginClip((int)layout.text.x,
                        y - metrics.text_guard / 2,
                        paragraph.width,
                        layout.text_clip_height + metrics.text_guard);
        Paragraph(paragraph, (int)layout.text.x, &y);
        if(layout.text_clipped)
            EndClip();
    }

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, guide.count);
    RenderText(page_text, (int)tip.x + metrics.pad,
                    layout.controls_y +
                        (metrics.button_size - metrics.page_font) / 2,
                    metrics.page_font, text_style.foreground);

    if(step > 0) {
        memset(&icon_props, 0, sizeof(icon_props));
        icon_props.bounds = layout.back_button;
        g_guide_debug.back_button = layout.back_button;
        icon_props.icon = guide.back_icon;
        icon_props.icon_size = metrics.nav_icon_size;
        icon_props.icon_padding = metrics.nav_icon_padding;
        if(RenderIconAction(icon_props)) {
            previous_requested = 1;
        }
    }
    memset(&icon_props, 0, sizeof(icon_props));
    icon_props.bounds = layout.next_button;
    g_guide_debug.next_button = layout.next_button;
    icon_props.icon = layout.finish ? guide.done_icon : guide.next_icon;
    icon_props.icon_size = metrics.nav_icon_size;
    icon_props.icon_padding = metrics.nav_icon_padding;
    if(RenderIconAction(icon_props)) {
        next_requested = 1;
    }

    policy = GuidePolicyFor(step, guide.count, previous_requested != 0,
                            next_requested != 0, 0);
    *guide.step = policy.step;
    result.step = policy.step;
    result.changed = policy.changed;
    result.closed = policy.closed;
    result.finished = policy.finished;
    return result;
}
