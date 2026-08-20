#ifndef UI_TK_H
#define UI_TK_H

#include "kryon_compat.generated.h"

#define UI_CLIPBOARD_BUFFER_SIZE 4096

typedef enum {
    UI_SIDE_TOP,
    UI_SIDE_BOTTOM,
    UI_SIDE_LEFT,
    UI_SIDE_RIGHT
} UISide;

typedef struct {
    Rectangle bounds;
    int pad_x;
    int pad_y;
    int gap;
    int cursor_x;
    int cursor_y;
} UIFrame;

typedef struct {
    Rectangle bounds;
    int rows;
    int cols;
    int gap_x;
    int gap_y;
    int pad_x;
    int pad_y;
} UIGrid;

typedef enum {
    UI_MENU_COMMAND,
    UI_MENU_CHECK,
    UI_MENU_RADIO,
    UI_MENU_SEPARATOR,
    UI_MENU_SUBMENU
} UIMenuItemKind;

typedef struct UIMenuItem {
    UIMenuItemKind kind;
    const char *label;
    const char *accelerator;
    int id;
    int disabled;
    int checked;
    const struct UIMenuItem *submenu;
    int submenu_count;
} UIMenuItem;

typedef struct {
    Rectangle bounds;
    const char *label;
    const UIMenuItem *items;
    int item_count;
} UIMenu;

typedef struct {
    int activated_id;
    int open_index;
} UIMenuBarResult;

typedef struct {
    char text[UI_CLIPBOARD_BUFFER_SIZE];
    int pending;
} UIClipboardBuffer;

typedef struct {
    int id;
    Rectangle trigger;
    const UIMenuItem *items;
    int item_count;
    int *open;
    int *x;
    int *y;
} UIContextMenu;

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
} UITableRow;

typedef struct {
    Rectangle bounds;
    int id;
    const char **columns;
    int column_count;
    const UITableRow *rows;
    int row_count;
    const int *column_widths;
    int *selected_row;
    int *sort_column;
    int *scroll_offset;
    int row_height;
} TableViewProps;

typedef struct {
    Rectangle bounds;
    int *scroll_x;
    int *scroll_y;
    float *zoom;
} UICanvas;

typedef struct {
    int active;
    int dragging;
    int selected_index;
    Vector2 world;
} UICanvasResult;

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
    int key;
    int ctrl;
    int shift;
    int alt;
    int id;
} UIAccelerator;

typedef struct {
    Rectangle bounds;
    const char *role;
    const char *label;
    int focused;
    int disabled;
    int checked;
} UIAccessibilityNode;

UIFrame BeginUIFrameBox(Rectangle bounds, int pad_x, int pad_y, int gap);
Rectangle UIFramePack(UIFrame *frame, UISide side, int size);
Rectangle UIGridCell(UIGrid grid, int row, int col, int row_span, int col_span);
Rectangle UIPlace(Rectangle parent, int x, int y, int w, int h);
UICanvasResult BeginUICanvas(UICanvas canvas);
void EndUICanvas(UICanvas canvas);
int UICanvasHitTest(Vector2 point, Rectangle *items, int item_count);
Vector2 UICanvasToScreen(UICanvas canvas, Vector2 point);
Rectangle UICanvasRectToScreen(UICanvas canvas, Rectangle rect);
int DrawUICascadingTreeView(CascadingTreeViewProps tree);

int UIAcceleratorPressed(UIAccelerator accelerator);
int DispatchUIAccelerators(const UIAccelerator *accelerators, int count);
int ContextMenu(UIContextMenu menu);
int SetUIClipboardTextValue(const char *text);
const char *GetUIClipboardTextValue(void);
int SetUIPrimarySelectionTextValue(const char *text);
const char *GetUIPrimarySelectionTextValue(void);
int UIClipboardTargetIncludes(const char *target, char wanted);
int UIClipboardTargetUsesPrimary(const char *target);
const char *GetUIClipboardTargetText(const UIClipboardBuffer *clipboard,
                                     const char *target);
int RequestUIClipboardTargetWrite(UIClipboardBuffer *clipboard,
                                  const char *target, const char *text);
void InitUIClipboardBuffer(UIClipboardBuffer *buffer, const char *text);
int SetUIClipboardBufferText(UIClipboardBuffer *buffer, const char *text);
int RequestUIClipboardBufferWrite(UIClipboardBuffer *buffer, const char *text);
const char *GetUIClipboardBufferText(const UIClipboardBuffer *buffer);
int UIClipboardBufferHasPendingWrite(const UIClipboardBuffer *buffer);
int SyncUIClipboardBufferFromHost(UIClipboardBuffer *buffer);
int FlushUIClipboardBufferToHost(UIClipboardBuffer *buffer);
#endif
