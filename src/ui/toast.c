#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/style.h"
#include "runtime/toast.h"

#define TOAST_MESSAGE_SIZE 256
static char toast_message[TOAST_MESSAGE_SIZE];
static int toast_class_name;
static double toast_until;

static void
copy_toast_message(const char *message)
{
    snprintf(toast_message, sizeof(toast_message), "%s", message ? message : "");
}

static void
ClearToast(void)
{
    toast_message[0] = '\0';
    toast_class_name = 0;
    toast_until = 0.0;
}

static void
format_toast_display(char *out, size_t out_size, const char *message,
                     ToastTruncation truncation)
{
    if(out_size == 0)
        return;
    if(message == NULL)
        message = "";
    if(!truncation.ellipsis) {
        snprintf(out, out_size, "%s", message);
        return;
    }
    if(truncation.prefix_len < 0)
        truncation.prefix_len = 0;
    snprintf(out, out_size, "%.*s...", truncation.prefix_len, message);
}

void
Toast(ToastProps props)
{
    ToastMetrics metrics = ToastMetricsFor(1.0f, (StyleFrame){0});
    ToastRequestDecision decision = ToastRequestDecisionFor(
        props.message != NULL && props.message[0] != '\0',
        (float)props.seconds, metrics);

    if(decision.clear) {
        ClearToast();
        return;
    }
    if(!decision.show)
        return;
    copy_toast_message(props.message);
    toast_class_name = props.class_name;
    toast_until = GetTime() + (double)decision.seconds;
}

void
RenderToast(void)
{
    int font = GetSmallFontSize();
    float scale = (float)Scale(1000) / 1000.0f;
    ToastMetrics metrics;
    ToastLayout layout;
    int text_w;
    int line_h;
    int content_w;
    char display[TOAST_MESSAGE_SIZE];
    ToastTruncation truncation;
    ToastRenderDecision decision = ToastRenderDecisionFor(
        toast_message[0] != '\0', (float)GetTime(), (float)toast_until);

    if(decision.clear) {
        ClearToast();
        return;
    }
    if(!decision.render)
        return;

    StyleData base = {.fields = (uint32_t)(StyleOpacity | StyleFontSize |
                                           StyleMaterial),
                      .opacity = 1.0f,
                      .font_size = (float)font,
                      .material = MaterialFlat};
    StyleFrame surface_frame = {
        .value = ResolveActiveStyle(
            base,
            StyleControlRoleFacts(StyleKindToast(), 0, toast_class_name,
                                  StyleAny(), ButtonToneNeutral,
                                  ButtonEmphasisSoft, ControlSizeMedium,
                                  ButtonStateNormal),
            ButtonStateNormal)};
    StyleFrame label_frame = {
        .value = ResolveActiveStyle(base,
                                    StyleControlRoleFacts(
                                        StyleKindToast(), 0,
                                        toast_class_name, ToastLabelRole(),
                                        ButtonToneNeutral,
                                        ButtonEmphasisSoft,
                                        ControlSizeMedium, ButtonStateNormal),
                                    ButtonStateNormal)};
    Style surface = ui_unpack_style(ui_style_apply_effects_frame(surface_frame).value);
    Style text = ui_unpack_style(ui_style_apply_effects_frame(label_frame).value);
    int font_token;
    metrics = ToastMetricsFor(scale, surface_frame);
    font = ResolveFont(0, StyleFontValue(text.fields, text.font_size), font);

    font_token = PushTextFont(text.typeface);
    content_w = ToastContentWidth(ui_view_width, metrics);
    snprintf(display, sizeof(display), "%s", toast_message);
    truncation = ToastTruncationFor((int)strlen(toast_message),
                                    TextWidth(display, font) <= content_w);
    format_toast_display(display, sizeof(display), toast_message, truncation);
    while(truncation.ellipsis && truncation.prefix_len > 0 &&
          TextWidth(display, font) > content_w) {
        truncation = ToastTruncationNext(truncation, 0);
        format_toast_display(display, sizeof(display), toast_message,
                             truncation);
    }

    text_w = TextWidth(display, font);
    line_h = TextLineHeight(font);
    layout = ToastLayoutFor(ui_view_width, ui_view_height, text_w, line_h,
                            metrics);

    ui_draw_material(layout.bounds, (Rectangle){0}, surface.background,
                     surface.border, surface.border, surface.radius,
                     surface.border_width, 0.0f, 0.0f, 0,
                     surface.focus, 0.0f, surface.opacity,
                     ui_style_fill(surface), surface.material);
    RenderText(display,
               (int)layout.text_bounds.x,
               (int)layout.text_bounds.y,
               font, Fade(text.foreground, text.opacity));
    PopTextFont(font_token);
}
