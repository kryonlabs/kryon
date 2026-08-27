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
    ButtonStyle style;
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
} UIForm;

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

UIForm UIFormBegin(int x, int y, int width);
int UIFormY(const UIForm *form);
int UIFormAdvance(UIForm *form, int height);
Rectangle UIFormTakeRect(UIForm *form, int height);
void UIFormNoteFocus(UIForm *form, int focus_id, Rectangle bounds);
int UIFormEnsureFocusedVisible(UIForm *form, UIScrollArea area, int margin);
int UIFormSectionLabel(UIForm *form, SectionLabelProps label);
int UIFormLabelTextField(UIForm *form, LabelTextFieldProps row);
int UIFormCheckboxRow(UIForm *form, CheckboxRowProps row);
int UIFormSpinboxRow(UIForm *form, SpinboxRowProps row);
int UIFormButtonRow(UIForm *form, ButtonRowProps row);

#endif
