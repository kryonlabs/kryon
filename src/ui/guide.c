#include "ui_internal.h"
#include <stdio.h>

static void
guide_draw_arrow(Rectangle tip, Rectangle anchor)
{
    int anchor_cx = (int)(anchor.x + anchor.width / 2);
    int anchor_cy = (int)(anchor.y + anchor.height / 2);
    int tip_left = (int)tip.x;
    int tip_right = (int)(tip.x + tip.width);
    int tip_top = (int)tip.y;
    int tip_bottom = (int)(tip.y + tip.height);
    int arrow_size = ScaleUIPx(10);
    Vector2 start, end;
    Color color = GetThemeText();

    if(anchor_cy < tip_top) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy + anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_top;
        DrawLineEx(start, end, (float)ScaleUIPx(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x - arrow_size, end.y - arrow_size},
                     (Vector2){end.x + arrow_size, end.y - arrow_size},
                     color);
    } else if(anchor_cy > tip_bottom) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy - anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_bottom;
        DrawLineEx(start, end, (float)ScaleUIPx(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x + arrow_size, end.y + arrow_size},
                     (Vector2){end.x - arrow_size, end.y + arrow_size},
                     color);
    } else if(anchor_cx < tip_left) {
        start.x = (float)(anchor_cx + anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_left;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)ScaleUIPx(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x - arrow_size, end.y - arrow_size},
                     (Vector2){end.x - arrow_size, end.y + arrow_size},
                     color);
    } else {
        start.x = (float)(anchor_cx - anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_right;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)ScaleUIPx(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x + arrow_size, end.y - arrow_size},
                     (Vector2){end.x + arrow_size, end.y + arrow_size},
                     color);
    }
}

static Rectangle
guide_tip_bounds(Rectangle anchor, int w, int h, int view_w, int view_h,
                 int reserved_top, int reserved_bottom)
{
    int margin = ScaleUIPx(12);
    int gap = ScaleUIPx(20);
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

    return (Rectangle){(float)x, (float)y, (float)w, (float)h};
}

UIGuideResult
DrawUIGuideOverlay(GuideOverlayProps guide)
{
    UIGuideResult result = {0};
    int view_w = guide.view_width > 0 ? guide.view_width : ui_view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : ui_view_height;
    int step;
    int margin = ScaleUIPx(12);
    int tip_w = view_w - margin * 2;
    int pad = ScaleUIPx(12);
    int button_size = ScaleUIPx(34);
    int close_size = ScaleUIPx(28);
    int page_font = Text12;
    int line_gap = guide.line_gap > 0 ? guide.line_gap : ScaleUIPx(6);
    int text_gap = ScaleUIPx(8);
    int controls_gap = ScaleUIPx(12);
    int max_tip_h;
    char page_text[32];
    ParagraphSpec paragraph;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    int text_clip_h;
    int controls_y;
    int finish;

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
    else if(tip_w > ScaleUIPx(300))
        tip_w = ScaleUIPx(300);

    paragraph = (ParagraphSpec){
        .text = guide.steps[step].text,
        .width = tip_w - pad * 2,
        .font = guide.paragraph_font,
        .line_gap = line_gap
    };
    paragraph_h = ui_paragraph_height(paragraph);
    tip_h = pad + close_size + text_gap + paragraph_h + controls_gap +
            button_size + pad;
    if(tip_h < ScaleUIPx(112))
        tip_h = ScaleUIPx(112);
    max_tip_h = view_h - guide.reserved_top - guide.reserved_bottom -
                margin * 2;
    if(max_tip_h < ScaleUIPx(112))
        max_tip_h = view_h - margin * 2;
    if(tip_h > max_tip_h)
        tip_h = max_tip_h;
    tip = guide_tip_bounds(guide.steps[step].anchor, tip_w, tip_h, view_w, view_h,
                           guide.reserved_top, guide.reserved_bottom);
    SetUIModalCapture(tip);

    DrawRectangle(0, 0, view_w, view_h, (Color){0, 0, 0, 86});
    DrawRectangleLinesEx(guide.steps[step].anchor, (float)ScaleUIPx(2),
                         GetThemeText());
    DrawRectangleRounded(tip, 0.08f, 8, GetThemeButton());
    DrawRectangleRoundedLines(tip, 0.08f, 8,
                              DarkenUIColor(GetThemeButton(), 35));
    guide_draw_arrow(tip, guide.steps[step].anchor);

    if(DrawUIIconButton((IconButtonProps){
           .bounds = {
               tip.x + tip.width - pad - close_size,
               tip.y + pad,
               (float)close_size,
               (float)close_size
           },
           .icon = guide.close_icon,
           .icon_size = ScaleUIPx(16),
           .icon_padding = ScaleUIPx(6)
       })) {
        result.closed = 1;
        return result;
    }

    y = (int)tip.y + pad + close_size + text_gap;
    controls_y = (int)tip.y + (int)tip.height - pad - button_size;
    text_clip_h = controls_y - controls_gap - y;
    if(text_clip_h > 0) {
        BeginUIClip((int)tip.x + pad, y, paragraph.width, text_clip_h);
        DrawUIParagraph(paragraph, (int)tip.x + pad, &y);
        EndUIClip();
    }

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, guide.count);
    DrawUIText(page_text, (int)tip.x + pad,
                    controls_y + (button_size - page_font) / 2,
                    page_font, GetThemeText());

    finish = step >= guide.count - 1;
    if(step > 0) {
        if(DrawUIIconButton((IconButtonProps){
               .bounds = {
                   tip.x + tip.width - pad - button_size * 2 - ScaleUIPx(8),
                   (float)controls_y,
                   (float)button_size,
                   (float)button_size
               },
               .icon = guide.back_icon,
               .icon_size = ScaleUIPx(19),
               .icon_padding = ScaleUIPx(7)
           })) {
            *guide.step = step - 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }
    if(DrawUIIconButton((IconButtonProps){
           .bounds = {
               tip.x + tip.width - pad - button_size,
               (float)controls_y,
               (float)button_size,
               (float)button_size
           },
           .icon = finish ? guide.done_icon : guide.next_icon,
           .icon_size = ScaleUIPx(19),
           .icon_padding = ScaleUIPx(7)
       })) {
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
