#include "ui_internal.h"
#include "runtime/toast.h"

#define UI_TOAST_MESSAGE_SIZE 256
#define UI_TOAST_DEFAULT_SECONDS 3.0

static char toast_message[UI_TOAST_MESSAGE_SIZE];
static double toast_until;

static void
copy_toast_message(const char *message)
{
    snprintf(toast_message, sizeof(toast_message), "%s", message ? message : "");
}

void
ClearToast(void)
{
    toast_message[0] = '\0';
    toast_until = 0.0;
}

void
ShowToastFor(const char *message, double seconds)
{
    ToastMetrics metrics = ToastMetricsFor(1.0f);

    if(message == NULL || message[0] == '\0') {
        ClearToast();
        return;
    }
    copy_toast_message(message);
    toast_until = GetTime() + (double)ToastDuration((float)seconds, metrics);
}

void
ShowToast(const char *message)
{
    ShowToastFor(message, UI_TOAST_DEFAULT_SECONDS);
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

    DrawRectangleRounded(layout.bounds, 0.18f, 12, DarkenColor(c_surface, 18));
    DrawRectangleRoundedLinesEx(layout.bounds, 0.18f, 12, Scale(1),
                                DarkenColor(c_surface, 46));
    RenderText(display,
               (int)layout.text_bounds.x,
               GetUIControlTextY(display, (int)layout.bounds.y,
                                 (int)layout.bounds.height, font),
               font, c_text);
}
