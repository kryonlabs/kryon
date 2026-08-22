#include "ui_internal.h"

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
    if(message == NULL || message[0] == '\0') {
        ClearToast();
        return;
    }
    if(seconds <= 0.0)
        seconds = UI_TOAST_DEFAULT_SECONDS;
    copy_toast_message(message);
    toast_until = GetTime() + seconds;
}

void
ShowToast(const char *message)
{
    ShowToastFor(message, UI_TOAST_DEFAULT_SECONDS);
}

void
DrawToast(void)
{
    DrawUIToast();
}

void
DrawUIToast(void)
{
    int font = GetUISmallFontSize();
    int pad_x = ScaleUIPx(14);
    int pad_y = ScaleUIPx(10);
    int margin = ScaleUIPx(18);
    int max_w = ui_view_width - margin * 2;
    int text_w;
    int content_w;
    int w;
    int h;
    int x;
    int y;
    Rectangle bounds;
    char display[UI_TOAST_MESSAGE_SIZE];

    if(toast_message[0] == '\0')
        return;
    if(GetTime() >= toast_until) {
        ClearToast();
        return;
    }

    snprintf(display, sizeof(display), "%s", toast_message);
    content_w = max_w - pad_x * 2;
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
    w = text_w + pad_x * 2;
    if(w > max_w)
        w = max_w;
    h = TextLineHeight(font) + pad_y * 2;
    x = (ui_view_width - w) / 2;
    y = ui_view_height - h - margin;
    bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};

    DrawRectangleRounded(bounds, 0.18f, 12, DarkenUIColor(c_surface, 18));
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 12, ScaleUIPx(1),
                                DarkenUIColor(c_surface, 46));
    DrawUIText(display,
               x + (w - text_w) / 2,
               GetUIControlTextY(display, y, h, font),
               font, c_text);
}
