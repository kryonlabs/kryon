#ifndef UI_TK_H
#define UI_TK_H

#include "kryon_compat.generated.h"

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
} Grid;

typedef enum {
    MenuCommand,
    MenuCheck,
    MenuRadio,
    MenuSeparator,
    MenuSubmenu
} MenuItemKind;

typedef struct MenuItem {
    MenuItemKind kind;
    const char *label;
    const char *accelerator;
    int id;
    int disabled;
    int checked;
    const struct MenuItem *submenu;
    int submenu_count;
} MenuItem;

typedef struct {
    Rectangle bounds;
    const char *label;
    const MenuItem *items;
    int item_count;
} Menu;

typedef struct {
    int activated_id;
    int open_index;
} MenuBarResult;

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
} RadioButtonProps;

typedef struct {
    Rectangle bounds;
    int min;
    int max;
    int value;
    const char *label;
} ProgressBarProps;

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
} ComboboxProps;

typedef struct {
    Rectangle bounds;
    const char *title;
} LabelFrameProps;

typedef struct {
    Rectangle bounds;
    Texture2D texture;
    Color tint;
} ImageBoxProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **items;
    int item_count;
    int *selected_index;
    int *scroll_offset;
    int row_height;
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
} TreeViewProps;

typedef struct {
    const char *label;
    int depth;
    int id;
    int is_dir;
    int selectable;
} UICascadingTreeItem;

typedef struct {
    int *ids;
    int *count;
    int capacity;
} UICascadingTreeExpansion;

typedef struct {
    Rectangle bounds;
    int id;
    const UICascadingTreeItem *items;
    int item_count;
    int *selected_id;
    int *activated_id;
    UICascadingTreeExpansion expanded;
    int *scroll_offset;
    int row_height;
} CascadingTreeViewProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int *scroll_x;
    int *scroll_y;
    int font_size;
    int line_height;
    int show_line_numbers;
} SourceViewProps;

typedef struct {
    const char **cells;
    int cell_count;
} TableRow;

typedef struct {
    Rectangle bounds;
    int id;
    const char **columns;
    int column_count;
    const TableRow *rows;
    int row_count;
    const int *column_widths;
    int *selected_row;
    int *selected_column;
    int *activated_row;
    int *activated_column;
    int *right_clicked_row;
    int *right_clicked_column;
    int *sort_column;
    int *scroll_offset;
    int row_height;
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
    const char **tabs;
    int tab_count;
    int *selected_index;
} NotebookProps;

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
    int *open;
} CollapsibleProps;

typedef struct {
    const char *title;
    const char *message;
    const char *ok_label;
} MessageDialogProps;

typedef struct {
    const char *title;
    const char **labels;
    Texture2D *icons;
    int option_count;
    const char *cancel_label;
    int max_width;
} PickerDialogProps;

typedef struct {
    const char *title;
    const char *message;
    const char *cancel_label;
    const char *confirm_label;
} ConfirmDialogProps;

typedef struct {
    const char *title;
    char *text;
    int text_size;
    int *cursor_position;
    int *focused;
    const char *cancel_label;
    const char *confirm_label;
} PromptDialogProps;

typedef struct {
    Rectangle anchor;
    const char *title;
    char *text;
    int text_size;
    int *cursor_position;
    int *focused;
    int id;
    int width;
    int max_codepoints;
} TextPopoverProps;

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
Rectangle GridCell(Grid grid, int row, int col, int row_span, int col_span);
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
