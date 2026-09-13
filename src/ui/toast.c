#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/toast.h"

#define UI_TOAST_MESSAGE_SIZE 256
#define UI_TOAST_DEFAULT_SECONDS 3.0

static char toast_message[UI_TOAST_MESSAGE_SIZE];
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

void
Toast(ToastProps props)
{
    ToastMetrics metrics = ToastMetricsFor(1.0f);

    if(props.message == NULL || props.message[0] == '\0') {
        ClearToast();
        return;
    }
    copy_toast_message(props.message);
    toast_class_name = props.class_name;
    toast_until = GetTime() + (double)ToastDuration((float)props.seconds,
                                                    metrics);
}

void
RenderToast(void)
{
    int font = GetSmallFontSize();
    float scale = (float)Scale(1000) / 1000.0f;
    ToastMetrics metrics = ToastMetricsFor(scale);
    ToastLayout layout;
    int text_w;
    int line_h;
    int content_w;
    char display[UI_TOAST_MESSAGE_SIZE];

    if(toast_message[0] == '\0')
        return;
    if(GetTime() >= toast_until) {
        ClearToast();
        return;
    }

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
                                        toast_class_name, 6,
                                        ButtonToneNeutral,
                                        ButtonEmphasisSoft,
                                        ControlSizeMedium, ButtonStateNormal),
                                    ButtonStateNormal)};
    Style surface = ui_unpack_style(ui_style_apply_effects_frame(surface_frame).value);
    Style text = ui_unpack_style(ui_style_apply_effects_frame(label_frame).value);
    int font_token;
    if(text.font_size > 0.0f)
        font = (int)(text.font_size + 0.5f);

    font_token = PushTextFont(text.typeface);
    snprintf(display, sizeof(display), "%s", toast_message);
    content_w = ToastContentWidth(ui_view_width, metrics);
    while(display[0] != '\0' && TextWidth(display, font) > content_w) {
        size_t len = strlen(display);
        if(len <= 3)
            break;
        snprintf(display + len - 3, 4, "...");
        if(TextWidth(display, font) <= content_w)
            break;
        display[len - 4] = '\0';
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
               GetUIControlTextY(display, (int)layout.bounds.y,
                                 (int)layout.bounds.height, font),
               font, Fade(text.foreground, text.opacity));
    PopTextFont(font_token);
}
