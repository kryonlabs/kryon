#ifndef UI_MODAL_H
#define UI_MODAL_H

#include "kryon_compat.generated.h"
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
} ParagraphModalMeasureProps;

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
    ButtonStyle style;
    int disabled;
} UIModalAction;

typedef struct {
    const char *title;
    const char *message;
    const UIModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} ModalProps;

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

#endif
