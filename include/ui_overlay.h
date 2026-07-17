#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "flint_compat.generated.h"

typedef struct {
    Rectangle anchor;
    const char *text;
} UIGuideStep;

typedef struct {
    const UIGuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} UIGuideOverlay;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} UIGuideResult;

UIGuideResult DrawUIGuideOverlay(UIGuideOverlay guide);
int DrawUIThemeSwitcher(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int DrawUIThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
int GetUIThemePickerHeight(int w);
void DrawUITutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void DrawUITutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);

#endif
