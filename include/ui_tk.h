#ifndef UI_TK_H
#define UI_TK_H

#include "flint_compat.generated.h"

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
    Rectangle bounds;
    const char *label;
    int id;
    int checked;
    int disabled;
} UIRadioButton;

typedef struct {
    Rectangle bounds;
    int min;
    int max;
    int value;
    const char *label;
} UIProgressBar;

typedef struct {
    Rectangle bounds;
    int id;
    int min;
    int max;
    int step;
    int *value;
    int disabled;
} UISpinbox;

typedef struct {
    Rectangle bounds;
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
} UICombobox;

typedef struct {
    Rectangle bounds;
    const char *title;
} UILabelFrame;

typedef struct {
    Rectangle bounds;
    Texture2D texture;
    Color tint;
} UIImageBox;

typedef struct {
    Rectangle bounds;
    int id;
    const char **items;
    int item_count;
    int *selected_index;
    int *scroll_offset;
    int row_height;
} UIListBox;

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
} UITreeView;

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
} UITableView;

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
} UINotebook;

typedef struct {
    Rectangle bounds;
    int id;
    int vertical;
    int *split;
    int min_first;
    int min_second;
} UIPanedView;

typedef struct {
    Rectangle bounds;
    const char *label;
    int *open;
} UICollapsible;

typedef struct {
    const char *title;
    const char *message;
    const char *ok_label;
} UIMessageDialog;

typedef struct {
    const char *title;
    const char *message;
    const char *cancel_label;
    const char *confirm_label;
} UIConfirmDialog;

typedef struct {
    const char *title;
    char *text;
    int text_size;
    int *cursor_position;
    int *focused;
    const char *cancel_label;
    const char *confirm_label;
} UIPromptDialog;

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
void DrawUISeparator(Rectangle bounds, int vertical);

UIMenuBarResult DrawUIMenuBar(int id, Rectangle bounds, const UIMenu *menus, int menu_count, int *open_index);
int DrawUIPopupMenu(int id, int x, int y, const UIMenuItem *items, int item_count);

int DrawUIRadioButton(UIRadioButton radio);
void DrawUIProgressBar(UIProgressBar progress);
int DrawUISpinbox(UISpinbox spinbox);
int DrawUICombobox(UICombobox combo);
void DrawUILabelFrame(UILabelFrame frame);
void DrawUIImageBox(UIImageBox image);

int DrawUIListBox(UIListBox list);
int DrawUITreeView(UITreeView tree);
int DrawUITableView(UITableView table);

UICanvasResult BeginUICanvas(UICanvas canvas);
void EndUICanvas(UICanvas canvas);
void DrawUICanvasGrid(Rectangle bounds, int step, Color color);
int UICanvasHitTest(Vector2 point, Rectangle *items, int item_count);
Vector2 UICanvasToScreen(UICanvas canvas, Vector2 point);
Rectangle UICanvasRectToScreen(UICanvas canvas, Rectangle rect);

int DrawUINotebook(UINotebook notebook);
int DrawUIPanedView(UIPanedView panes);
int DrawUICollapsible(UICollapsible section);

int DrawUIMessageDialog(UIMessageDialog dialog);
int DrawUIConfirmDialog(UIConfirmDialog dialog);
int DrawUIPromptDialog(UIPromptDialog dialog);
int DrawUIColorPicker(Rectangle bounds, Color *color);

int UIAcceleratorPressed(UIAccelerator accelerator);
int DispatchUIAccelerators(const UIAccelerator *accelerators, int count);
int SetUIClipboardTextValue(const char *text);
const char *GetUIClipboardTextValue(void);
void DrawUIFocusDebugOverlay(const UIAccessibilityNode *nodes, int count);

#endif
