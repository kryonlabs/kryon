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
guide_draw_arrow(Rectangle tip, Rectangle anchor, Color color,
                 GuideMetrics metrics)
{
    GuideArrow arrow = GuideArrowFor(tip, anchor, metrics);

    if(arrow.stroke_width > 0.0f)
        DrawLineEx(arrow.line_start, arrow.line_end, arrow.stroke_width, color);
    if(metrics.gap > 0)
        DrawTriangle(arrow.tip0, arrow.tip1, arrow.tip2, color);
}

static StyleFrame
guide_frame(int role, ButtonState state)
{
    return ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .icon_only = true},
        state, 0, 0.0f, 0.0f, 0.0f, StyleKindGuide(), role);
}

static Style
guide_style(int role, ButtonState state)
{
    return ui_unpack_style(guide_frame(role, state).value);
}

static GuideMetrics
guide_metrics(void)
{
    return GuideMetricsFor((float)GetScale(),
                           guide_frame(2, ButtonStateNormal),
                           guide_frame(24, ButtonStateNormal),
                           guide_frame(6, ButtonStateNormal),
                           guide_frame(17, ButtonStateNormal),
                           guide_frame(15, ButtonStateNormal));
}

static IconActionSpec
guide_icon_action(Rectangle bounds, Texture2D icon, int size, int padding,
                  Style normal, Style hover)
{
    IconActionSpec props = {0};
    props.bounds = bounds;
    props.icon = icon;
    props.icon_size = size;
    props.icon_padding = padding;
    props.background = normal.background;
    props.hover_background = hover.background;
    props.icon_color = normal.foreground;
    props.border = normal.border;
    props.radius = normal.radius;
    return props;
}

GuideResult
RenderGuideOverlay(GuideOverlayProps guide)
{
    GuideResult result = {0};
    GuideMetrics metrics = guide_metrics();
    GuideLayout layout;
    GuidePolicy policy;
    int view_w = guide.view_width > 0 ? guide.view_width : ui_view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : ui_view_height;
    int step;
    int tip_w;
    int line_gap = guide.line_gap > 0 ? guide.line_gap : metrics.default_line_gap;
    int max_tip_h;
    char page_text[32];
    int label_font;
    ParagraphSpec paragraph;
    Color label_color;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    IconActionSpec icon_props;
    int previous_requested = 0;
    int next_requested = 0;
    int close_requested = 0;
    Style panel_style = guide_style(2, ButtonStateNormal);
    Style label_style = guide_style(6, ButtonStateNormal);
    Style scrim_style = guide_style(19, ButtonStateNormal);
    Style anchor_style = guide_style(24, ButtonStateNormal);
    Style close_style = guide_style(15, ButtonStateNormal);
    Style close_hover_style = guide_style(15, ButtonStateHover);
    Style action_style = guide_style(17, ButtonStateNormal);
    Style action_hover_style = guide_style(17, ButtonStateHover);

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
    if((label_style.fields & (uint32_t)StyleFontSize) != 0 &&
       label_style.font_size > 0.0f)
        paragraph.font = (int)(label_style.font_size + 0.5f);
    label_font = paragraph.font;
    paragraph.line_gap = line_gap;
    label_color = GetColor(Opacity(ColorToInt(label_style.foreground),
                                   label_style.opacity));
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

    guide_draw_scrim(GuideScrimFor(view_w, view_h, guide.steps[step].anchor,
                                   metrics),
                     GetColor(Opacity(ColorToInt(scrim_style.background),
                                      scrim_style.opacity)));
    DrawRectangleLinesEx(guide.steps[step].anchor,
                         (float)metrics.anchor_stroke,
                         anchor_style.border);
    guide_draw_arrow(tip, guide.steps[step].anchor, anchor_style.foreground,
                     metrics);

    ui_draw_material(tip, (Rectangle){0}, panel_style.background,
                     panel_style.border, panel_style.border,
                     panel_style.radius, panel_style.border_width,
                     0.0f, 0.0f, 0, panel_style.focus, 0.0f,
                     panel_style.opacity, ui_style_fill(panel_style),
                     panel_style.material);

    icon_props = guide_icon_action(layout.close_button, guide.close_icon,
                                   metrics.close_icon_size,
                                   metrics.close_icon_padding,
                                   close_style, close_hover_style);
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
                        (metrics.button_size - label_font) / 2,
                    label_font, label_color);

    if(step > 0) {
        g_guide_debug.back_button = layout.back_button;
        icon_props = guide_icon_action(layout.back_button, guide.back_icon,
                                       metrics.nav_icon_size,
                                       metrics.nav_icon_padding,
                                       action_style, action_hover_style);
        if(RenderIconAction(icon_props)) {
            previous_requested = 1;
        }
    }
    g_guide_debug.next_button = layout.next_button;
    icon_props = guide_icon_action(layout.next_button,
                                   layout.finish ? guide.done_icon : guide.next_icon,
                                   metrics.nav_icon_size,
                                   metrics.nav_icon_padding,
                                   action_style, action_hover_style);
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
