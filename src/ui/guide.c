#include "ui_internal.h"
#include <stdio.h>

static UIGuideOverlayDebug g_ui_guide_debug;

static void
guide_draw_scrim(int view_w, int view_h, Rectangle anchor, Color scrim)
{
    int padding = Scale(4);
    int left = ui_clampi((int)anchor.x - padding, 0, view_w);
    int top = ui_clampi((int)anchor.y - padding, 0, view_h);
    int right = ui_clampi((int)(anchor.x + anchor.width) + padding, 0, view_w);
    int bottom = ui_clampi((int)(anchor.y + anchor.height) + padding, 0, view_h);

    DrawRectangle(0, 0, view_w, top, scrim);
    DrawRectangle(0, bottom, view_w, view_h - bottom, scrim);
    DrawRectangle(0, top, left, bottom - top, scrim);
    DrawRectangle(right, top, view_w - right, bottom - top, scrim);
}

int
GetUIGuideOverlayDebug(UIGuideOverlayDebug *out)
{
    if(out != NULL)
        *out = g_ui_guide_debug;
    return g_ui_guide_debug.valid;
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
    Color color = GetThemeText();

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

static Rectangle
guide_tip_bounds(Rectangle anchor, int w, int h, int view_w, int view_h,
                 int reserved_top, int reserved_bottom)
{
    int margin = Scale(12);
    int gap = Scale(20);
    int bottom = view_h - reserved_bottom;
    int x = (int)(anchor.x + anchor.width / 2) - w / 2;
    int y;

    if(bottom < reserved_top + margin)
        bottom = view_h - margin;
    if(x < margin)
        x = margin;
    if(x + w > view_w - margin)
        x = view_w - margin - w;
    if(x < margin)
        x = margin;

    if(anchor.y + anchor.height + gap + h < bottom)
        y = (int)(anchor.y + anchor.height + gap);
    else
        y = (int)(anchor.y - gap - h);

    if(y < reserved_top + margin)
        y = reserved_top + margin;
    if(y + h > bottom - margin)
        y = bottom - margin - h;
    if(y < margin)
        y = margin;

    {
        Rectangle rect;

        rect.x = (float)x;
        rect.y = (float)y;
        rect.width = (float)w;
        rect.height = (float)h;
        return rect;
    }
}

UIGuideResult
RenderGuideOverlay(GuideOverlayProps guide)
{
    UIGuideResult result = {0};
    int view_w = guide.view_width > 0 ? guide.view_width : ui_view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : ui_view_height;
    int step;
    int margin = Scale(12);
    int tip_w = view_w - margin * 2;
    int pad = Scale(12);
    int button_size = Scale(34);
    int close_size = Scale(28);
    int page_font = Text12;
    int line_gap = guide.line_gap > 0 ? guide.line_gap : Scale(6);
    int text_gap = Scale(8);
    int controls_gap = Scale(12);
    int text_guard = Scale(8);
    int tip_chrome_h;
    int max_tip_h;
    char page_text[32];
    ParagraphSpec paragraph;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    Rectangle close_button = {0};
    Rectangle back_button = {0};
    Rectangle next_button = {0};
    int y;
    int text_clip_h;
    int controls_y;
    int finish;
    Color scrim;
    Color panel;
    Color panel_border;
    IconButtonProps icon_props;

    g_ui_guide_debug.valid = 0;
    if(guide.steps == NULL || guide.count <= 0 || guide.step == NULL)
        return result;

    step = ui_clampi(*guide.step, 0, guide.count - 1);
    *guide.step = step;
    result.step = step;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(step >= guide.count - 1) {
            result.finished = 1;
        } else {
            *guide.step = step + 1;
            result.changed = 1;
            result.step = *guide.step;
        }
        return result;
    }
    if(IsKeyPressed(KEY_LEFT) && step > 0) {
        *guide.step = step - 1;
        result.changed = 1;
        result.step = *guide.step;
        return result;
    }
    if(IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) {
        result.closed = 1;
        return result;
    }

    if(guide.max_width > 0 && tip_w > guide.max_width)
        tip_w = guide.max_width;
    else if(tip_w > Scale(300))
        tip_w = Scale(300);

    max_tip_h = view_h - guide.reserved_top - guide.reserved_bottom -
                margin * 2;
    if(max_tip_h < Scale(112))
        max_tip_h = view_h - margin * 2;
    tip_chrome_h = pad + close_size + text_gap + text_guard + controls_gap +
                   button_size + pad;

    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = guide.steps[step].text;
    paragraph.width = tip_w - pad * 2;
    paragraph.font = guide.paragraph_font > 0 ? guide.paragraph_font : Text16;
    paragraph.line_gap = line_gap;
    paragraph_h = ui_paragraph_height(paragraph);
    while(paragraph.font > Text12 &&
          paragraph_h > max_tip_h - tip_chrome_h) {
        paragraph.font--;
        paragraph_h = ui_paragraph_height(paragraph);
    }

    tip_h = tip_chrome_h + paragraph_h;
    if(tip_h < Scale(112))
        tip_h = Scale(112);
    if(tip_h > max_tip_h)
        tip_h = max_tip_h;
    tip = guide_tip_bounds(guide.steps[step].anchor, tip_w, tip_h, view_w, view_h,
                           guide.reserved_top, guide.reserved_bottom);
    SetUIModalCapture(tip);

    scrim.r = 0;
    scrim.g = 0;
    scrim.b = 0;
    scrim.a = 86;
    guide_draw_scrim(view_w, view_h, guide.steps[step].anchor, scrim);
    DrawRectangleLinesEx(guide.steps[step].anchor, (float)Scale(2),
                         GetThemeText());
    guide_draw_arrow(tip, guide.steps[step].anchor);

    panel = GetThemeSurface();
    panel.a = 255;
    panel_border = Fade(GetThemeText(), 0.22f);
    DrawRectangleRounded(tip, 0.08f, 8, panel);
    DrawRectangleRoundedLines(tip, 0.08f, 8, panel_border);

    memset(&icon_props, 0, sizeof(icon_props));
    close_button.x = tip.x + tip.width - pad - close_size;
    close_button.y = tip.y + pad;
    close_button.width = (float)close_size;
    close_button.height = (float)close_size;
    icon_props.bounds = close_button;
    icon_props.icon = guide.close_icon;
    icon_props.icon_size = Scale(16);
    icon_props.icon_padding = Scale(6);
    if(RenderIconButton(icon_props)) {
        result.closed = 1;
        return result;
    }

    y = (int)tip.y + pad + close_size + text_gap;
    controls_y = (int)tip.y + (int)tip.height - pad - button_size;
    text_clip_h = controls_y - controls_gap - y;
    g_ui_guide_debug.valid = 1;
    g_ui_guide_debug.step = step;
    g_ui_guide_debug.count = guide.count;
    g_ui_guide_debug.paragraph_height = paragraph_h;
    g_ui_guide_debug.text_clip_height = text_clip_h;
    g_ui_guide_debug.text_clipped = text_clip_h < paragraph_h + text_guard;
    g_ui_guide_debug.tip = tip;
    g_ui_guide_debug.text = (Rectangle){(float)((int)tip.x + pad),
                                        (float)y,
                                        (float)paragraph.width,
                                        (float)text_clip_h};
    g_ui_guide_debug.close_button = close_button;
    g_ui_guide_debug.back_button = back_button;
    if(text_clip_h > 0) {
        if(text_clip_h < paragraph_h + text_guard)
            BeginUIClip((int)tip.x + pad, y - text_guard / 2,
                        paragraph.width, text_clip_h + text_guard);
        Paragraph(paragraph, (int)tip.x + pad, &y);
        if(text_clip_h < paragraph_h + text_guard)
            EndUIClip();
    }

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, guide.count);
    RenderText(page_text, (int)tip.x + pad,
                    controls_y + (button_size - page_font) / 2,
                    page_font, GetThemeText());

    finish = step >= guide.count - 1;
    if(step > 0) {
        memset(&icon_props, 0, sizeof(icon_props));
        back_button.x = tip.x + tip.width - pad - button_size * 2 - Scale(8);
        back_button.y = (float)controls_y;
        back_button.width = (float)button_size;
        back_button.height = (float)button_size;
        icon_props.bounds = back_button;
        g_ui_guide_debug.back_button = back_button;
        icon_props.icon = guide.back_icon;
        icon_props.icon_size = Scale(19);
        icon_props.icon_padding = Scale(7);
        if(RenderIconButton(icon_props)) {
            *guide.step = step - 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }
    memset(&icon_props, 0, sizeof(icon_props));
    next_button.x = tip.x + tip.width - pad - button_size;
    next_button.y = (float)controls_y;
    next_button.width = (float)button_size;
    next_button.height = (float)button_size;
    icon_props.bounds = next_button;
    g_ui_guide_debug.next_button = next_button;
    icon_props.icon = finish ? guide.done_icon : guide.next_icon;
    icon_props.icon_size = Scale(19);
    icon_props.icon_padding = Scale(7);
    if(RenderIconButton(icon_props)) {
        if(finish) {
            result.finished = 1;
        } else {
            *guide.step = step + 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }

    return result;
}
