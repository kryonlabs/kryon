#ifndef UI_ROWS_H
#define UI_ROWS_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_scroll.h"
#include "ui_tk.h"

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
} InfoRowsProps;

typedef struct {
    const char *label;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    int disabled;
} ButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const ButtonRowItem *items;
    int count;
} ButtonRowProps;

typedef struct {
    const char *label;
    TextFieldProps field;
    int label_font;
    int label_h;
    int field_h;
    int gap;
    int bottom_gap;
    Color label_color;
} LabelTextFieldProps;

typedef struct {
    const char *label;
    int font;
    int info_button;
    int icon_diameter;
    int height;
    Color color;
} SectionLabelProps;

typedef struct {
    const char *label;
    int *value;
    int height;
    int disabled;
} CheckboxRowProps;

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
} OverlayButtonProps;

typedef struct {
    int x;
    int y;
    int width;
    int cursor_y;
    int gap;
    Rectangle last_bounds;
    int focused_rect_valid;
    Rectangle focused_rect;
} Form;

typedef struct {
    const char *label;
    SpinboxProps spinbox;
    int label_font;
    int label_width;
    int row_height;
    int control_width;
    Color label_color;
} SpinboxRowProps;

int GetUILabelTextFieldHeight(LabelTextFieldProps row);
int GetUIButtonRowHeight(ButtonRowProps row);
int GetUISpinboxRowHeight(SpinboxRowProps row);

Form FormBegin(int x, int y, int width);
int FormY(const Form *form);
int FormAdvance(Form *form, int height);
Rectangle FormTakeRect(Form *form, int height);
void FormNoteFocus(Form *form, int focus_id, Rectangle bounds);
int FormEnsureFocusedVisible(Form *form, UIScrollArea area, int margin);
int FormSection(Form *form, SectionLabelProps row);
int FormTextField(Form *form, LabelTextFieldProps row);
int FormCheckbox(Form *form, CheckboxRowProps row);
int FormSpinbox(Form *form, SpinboxRowProps row);
int FormButtons(Form *form, ButtonRowProps row);

#endif
