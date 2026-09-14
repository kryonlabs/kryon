#ifndef KRYON_UI_ROWS_INTERNAL_H
#define KRYON_UI_ROWS_INTERNAL_H

#include "ui_controls.h"
#include "ui_scroll_internal.h"
#include "ui_tk.h"

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
    int info;
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
    int x;
    int y;
    int width;
    int cursor_y;
    int gap;
    Rectangle last_bounds;
    int focused_rect_valid;
    Rectangle focused_rect;
} RowForm;

typedef struct {
    const char *label;
    SpinboxProps spinbox;
    int label_font;
    int label_width;
    int row_height;
    int control_width;
    Color label_color;
} SpinboxRowProps;

int GetLabelTextFieldHeight(LabelTextFieldProps row);
int GetButtonRowHeight(ButtonRowProps row);
int GetSpinboxRowHeight(SpinboxRowProps row);

RowForm RowFormBegin(int x, int y, int width);
int RowFormY(const RowForm *form);
int RowFormAdvance(RowForm *form, int height);
Rectangle RowFormTakeRect(RowForm *form, int height);
void RowFormNoteFocus(RowForm *form, int focus_id, Rectangle bounds);
int RowFormEnsureFocusedVisible(RowForm *form, ScrollArea area, int margin);
int RowFormSection(RowForm *form, SectionLabelProps row);
int RowFormTextField(RowForm *form, LabelTextFieldProps row);
int RowFormCheckbox(RowForm *form, CheckboxRowProps row);
int RowFormSpinbox(RowForm *form, SpinboxRowProps row);
int RowFormButtons(RowForm *form, ButtonRowProps row);

#endif
