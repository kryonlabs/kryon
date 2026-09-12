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
} TitleBarDropdown;

typedef struct {
    const char *title;
    int height;
    Texture2D leading_icon;
    int has_leading_action;
    TitleBarDropdown dropdown;
    int has_dropdown;
} TitleBarProps;

typedef struct {
    const char *label;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    int disabled;
} ModalAction;

typedef struct {
    const char *title;
    const char *message;
    const ModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
    char *text;
    int text_size;
    int *cursor_position;
    int *focused;
    int focus_id;
} ModalProps;

#endif
