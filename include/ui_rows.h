#ifndef UI_ROWS_H
#define UI_ROWS_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"

typedef struct {
    const char *text;
    int font;
    Color color;
} UIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const UIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} UIInfoRows;

typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const UIButtonRowItem *items;
    int count;
} UIButtonRow;

typedef struct {
    const char *label;
    UITextField field;
    int label_font;
    int label_h;
    int field_h;
    int gap;
    int bottom_gap;
    Color label_color;
} UILabelTextField;

typedef struct {
    const char *label;
    int font;
    int info_button;
    int icon_diameter;
    int height;
    Color color;
} UISectionLabel;

typedef struct {
    const char *label;
    int *value;
    int height;
    int disabled;
} UICheckboxRow;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int disabled;
    Color background;
    Color hover_background;
    Color border;
    Color hover_border;
    Color text;
} UIOverlayButton;

#endif
