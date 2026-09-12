#ifndef UI_TK_H
#define UI_TK_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_menu_types.h"

#define UI_CLIPBOARD_BUFFER_SIZE 4096

typedef enum {
    SideTop,
    SideBottom,
    SideLeft,
    SideRight
} Side;

typedef struct {
    Rectangle bounds;
    int pad_x;
    int pad_y;
    int gap;
    int cursor_x;
    int cursor_y;
} FrameBox;

typedef struct {
    Rectangle bounds;
    int rows;
    int cols;
    int gap_x;
    int gap_y;
    int pad_x;
    int pad_y;
} GridFrame;

typedef struct {
    char text[UI_CLIPBOARD_BUFFER_SIZE];
    int pending;
} UIClipboardBuffer;

typedef enum {
    UI_CLIPBOARD_SOURCE_CLIPBOARD,
    UI_CLIPBOARD_SOURCE_PRIMARY,
    UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD
} UIClipboardSource;

typedef int (*UIClipboardOSC52WriteFn)(void *userdata, const char *text);
typedef int (*UIClipboardPasteWriteFn)(void *userdata, const char *text,
                                       int size);

typedef struct {
    int id;
    Rectangle trigger;
    const MenuItem *items;
    int item_count;
    int *open;
    int *x;
    int *y;
} ContextMenuProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    int id;
    int checked;
    int disabled;
} RadioProps;

typedef struct {
    Rectangle bounds;
    int id;
    int *value;
    const char *off_label;
    const char *on_label;
    int disabled;
} ToggleProps;

typedef struct {
    Rectangle bounds;
    int min;
    int max;
    int value;
    const char *label;
} ProgressProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    const float *values;
    int value_count;
    int offset;
    const char *overlay;
    float scale_min;
    float scale_max;
    int mode;
} PlotProps;

typedef enum {
    NumericFloat = 0,
    NumericInt = 1,
    NumericDouble = 2
} NumericValueKind;

typedef enum {
    DragValue = 0,
    DragRange = 1
} DragMode;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    NumericValueKind kind;
    DragMode mode;
    float *float_values;
    int *int_values;
    int value_count;
    float *float_min;
    float *float_max;
    int *int_min;
    int *int_max;
    float speed;
    double min;
    double max;
    const char *format;
    const char *format_max;
    int disabled;
} DragProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    NumericValueKind kind;
    float *float_values;
    int *int_values;
    int value_count;
    float *float_value;
    double min;
    double max;
    const char *format;
    int disabled;
    int vertical;
    int angle;
} SliderProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    NumericValueKind kind;
    float *float_values;
    int *int_values;
    double *double_values;
    int value_count;
    double step;
    double step_fast;
    const char *format;
    int disabled;
} InputProps;

typedef struct {
    Rectangle bounds;
    int id;
    int disabled;
} InvisibleButtonProps;

typedef enum {
    TextWrapAuto = 0,
    TextWrapNone
} TextWrap;

typedef enum {
    TextAlignStart = 0,
    TextAlignCenter,
    TextAlignEnd
} TextAlign;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    Color color;
    TextWrap wrap;
    TextAlign align;
    TextAlign vertical_align;
    int disabled;
    int letter_spacing;
    const char *typeface;
    Style style;
} TextProps;

typedef struct {
    Rectangle bounds;
    int vertical;
    const char *label;
    int font;
    int disabled;
} SeparatorProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *type;
    const void *data;
    int data_size;
    int disabled;
} DragDropSourceProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *type;
    void *output;
    int output_size;
    int *accepted_size;
    int disabled;
} DragDropTargetProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **items;
    int item_count;
    int *selected;
    int *selected_count;
    int *anchor;
    int row_height;
    int disabled;
} MultiSelectListProps;

typedef enum {
    ARROW_LEFT = 0,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
} ArrowDirection;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *values;
    int value_count;
    int disabled;
    int picker;
} ColorPickerProps;

typedef struct {
    Rectangle bounds;
    int id;
    int min;
    int max;
    int step;
    int *value;
    int disabled;
    const char *value_text;
    int wrap;
} SpinboxProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    const DropdownOption *items;
} DropdownProps;

typedef enum {
    PopupFlagsNone = 0,
    PopupTooltip = 1 << 0,
    PopupModal = 1 << 1,
    PopupContext = 1 << 2
} PopupFlags;

typedef struct {
    Rectangle bounds;
    int id;
    bool *open;
    int disabled;
    Rectangle trigger;
    unsigned int flags;
} PopupProps;

typedef struct {
    Rectangle bounds;
    const char *title;
} FieldsetProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **items;
    int item_count;
    int *selected_index;
    int *scroll_offset;
    int row_height;
    int disabled;
    int content_height;
} ListBoxProps;

typedef struct {
    const char *label;
    int depth;
    int id;
    int expanded;
    int selectable;
} UITreeItem;

typedef struct {
    Rectangle bounds;
    int id;
    const UITreeItem *items;
    int item_count;
    int *selected_id;
    int *scroll_offset;
    int row_height;
    int disabled;
} TreeViewProps;

typedef struct {
    const char **cells;
    int cell_count;
    const Color *text_colors;
    const Color *background_colors;
} TableRow;

typedef struct {
    Rectangle bounds;
    int id;
    const char **columns;
    int column_count;
    const TableRow *rows;
    int row_count;
    int *column_widths;
    int *selected_row;
    int *selected_column;
    int *activated_row;
    int *activated_column;
    int *right_clicked_row;
    int *right_clicked_column;
    int *sort_column;
    int *scroll_offset;
    int row_height;
    const int *column_enabled;
    const int *column_order;
    int *sort_direction;
    int disabled;
    int resizable;
    int min_column_width;
    int freeze_rows;
    int header_height;
    float header_angle;
    int custom_cells;
    const char *copy_text;
    const char **pasted_text;
    int *pasted_row;
    int *pasted_column;
} TableViewProps;

typedef struct {
    Rectangle bounds;
    int *scroll_x;
    int *scroll_y;
    float *zoom;
} Canvas;

typedef struct {
    int active;
    int dragging;
    int selected_index;
    Vector2 world;
} CanvasResult;

typedef struct {
    Rectangle bounds;
    int id;
    int vertical;
    int *split;
    int min_first;
    int min_second;
} PanedViewProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    bool *open;
    /* Tree-style headers indent by depth; callers lay out conditional children.
       Leaves never toggle open. Return value remains whether open changed. */
    int tree;
    int depth;
    int leaf;
    int selected;
    int disabled;
    int id;
    /* Optional ImGui-style close state. A false value hides the header;
       activating the close affordance sets it false without toggling open. */
    bool *visible;
} CollapsibleProps;

typedef struct {
    int key;
    int ctrl;
    int shift;
    int alt;
    int id;
} Accelerator;

typedef struct {
    Rectangle bounds;
    const char *role;
    const char *label;
    int focused;
    int disabled;
    int checked;
} UIAccessibilityNode;

FrameBox BeginFrameBox(Rectangle bounds, int pad_x, int pad_y, int gap);
Rectangle FramePack(FrameBox *frame, Side side, int size);
Rectangle GridCell(GridFrame grid, int row, int col, int row_span, int col_span);
Rectangle Place(Rectangle parent, int x, int y, int w, int h);
CanvasResult BeginCanvas(Canvas canvas);
void EndCanvas(Canvas canvas);
int CanvasHitTest(Vector2 point, Rectangle *items, int item_count);
Vector2 CanvasToScreen(Canvas canvas, Vector2 point);
Rectangle CanvasRectToScreen(Canvas canvas, Rectangle rect);

int AcceleratorPressed(Accelerator accelerator);
int DispatchAccelerators(const Accelerator *accelerators, int count);
int ContextMenu(ContextMenuProps menu);
int SetUIClipboardTextValue(const char *text);
const char *GetUIClipboardTextValue(void);
int SetUIPrimarySelectionTextValue(const char *text);
const char *GetUIPrimarySelectionTextValue(void);
int UIClipboardSourceHasText(UIClipboardSource source);
const char *GetUIClipboardSourceText(const UIClipboardBuffer *clipboard,
                                     UIClipboardSource source);
int SetUIPrimarySelectionFromText(const char *text);
int CopyUISelectionTextToClipboard(UIClipboardBuffer *clipboard,
                                   const char *text);
int UIClipboardTargetIncludes(const char *target, char wanted);
int UIClipboardTargetUsesPrimary(const char *target);
const char *GetUIClipboardTargetText(const UIClipboardBuffer *clipboard,
                                     const char *target);
int RequestUIClipboardTargetWrite(UIClipboardBuffer *clipboard,
                                  const char *target, const char *text);
int HandleUIClipboardOSC52(UIClipboardBuffer *clipboard, const char *payload,
                           UIClipboardOSC52WriteFn write_response,
                           void *userdata);
int WriteUIClipboardPaste(const char *text, int bracketed,
                          UIClipboardPasteWriteFn write_text,
                          void *userdata);
int WriteUIClipboardTextPaste(UIClipboardBuffer *clipboard, const char *text,
                              int bracketed, UIClipboardPasteWriteFn write_text,
                              void *userdata);
int WriteUIClipboardSourcePaste(UIClipboardBuffer *clipboard,
                                UIClipboardSource source, int bracketed,
                                UIClipboardPasteWriteFn write_text,
                                void *userdata);
void InitUIClipboardBuffer(UIClipboardBuffer *buffer, const char *text);
int SetUIClipboardBufferText(UIClipboardBuffer *buffer, const char *text);
int RequestUIClipboardBufferWrite(UIClipboardBuffer *buffer, const char *text);
const char *GetUIClipboardBufferText(const UIClipboardBuffer *buffer);
int UIClipboardBufferHasPendingWrite(const UIClipboardBuffer *buffer);
int SyncUIClipboardBufferFromHost(UIClipboardBuffer *buffer);
int FlushUIClipboardBufferToHost(UIClipboardBuffer *buffer);
#endif
