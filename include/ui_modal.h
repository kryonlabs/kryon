#ifndef UI_MODAL_H
#define UI_MODAL_H

#include "flint_compat.generated.h"
#include "ui_controls.h"

typedef struct {
    const char *message;
    int width;
    int header_h;
    int button_h;
    int line_gap;
    int extra_lines;
    int min_height;
    int font;
} UIParagraphModalMeasure;

typedef struct {
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    int min_width;
    int height;
} UITitleBarDropdown;

typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIModalAction;

typedef struct {
    const char *title;
    const char *message;
    const UIModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} UIModalSpec;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} UIPanelFrame;

int DrawUIActionModal(UIModalSpec modal);
int DrawUIModal(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int DrawUIModal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
int GetUIParagraphModalHeight(UIParagraphModalMeasure measure);
int GetUITitleBarHeight(void);
void DrawUITitleBar(const char *title, int height);
int DrawUIReturnTitleBar(Texture2D return_icon, const char *title,
                         int height);
int DrawUIReturnDropdownTitleBar(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame DrawUIModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);

#endif
