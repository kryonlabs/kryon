#ifndef KRYON_TK_H
#define KRYON_TK_H

#include "kryon_compat.generated.h"
#include "ui_collapsible_props.generated.h"
#include "ui_color_picker_props.generated.h"
#include "ui_drag_props.generated.h"
#include "ui_fieldset_props.generated.h"
#include "ui_input_props.generated.h"
#include "ui_paned_view_props.generated.h"
#include "ui_plot_props.generated.h"
#include "ui_progress_props.generated.h"
#include "ui_radio_props.generated.h"
#include "ui_scroll_props.generated.h"
#include "ui_separator_props.generated.h"
#include "ui_slider_props.generated.h"
#include "ui_spinbox_props.generated.h"
#include "ui_toggle_props.generated.h"
#include "ui_controls.h"
#include "ui_menu_types.h"

#define CLIPBOARD_BUFFER_SIZE 4096

typedef struct {
    char text[CLIPBOARD_BUFFER_SIZE];
    int pending;
} ClipboardBuffer;

typedef enum {
    CLIPBOARD_SOURCE_CLIPBOARD,
    CLIPBOARD_SOURCE_PRIMARY,
    CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD
} ClipboardSource;

typedef int (*ClipboardOSC52WriteFn)(void *userdata, const char *text);
typedef int (*ClipboardPasteWriteFn)(void *userdata, const char *text,
                                       int size);

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
    int class_name;
    int font;
    Color color;
    TextWrap wrap;
    TextAlign align;
    TextAlign vertical_align;
    int disabled;
    int letter_spacing;
    int selectable;
    const char *typeface;
    Style style;
} TextProps;

typedef enum {
    DragDropRoleSource = 0,
    DragDropRoleTarget = 1
} DragDropRole;

typedef struct {
    Rectangle bounds;
    int id;
    DragDropRole role;
    const char *type;
    const void *data;
    int data_size;
    void *output;
    int output_size;
    int *accepted_size;
    int disabled;
} DragDropProps;

typedef enum {
    ARROW_LEFT = 0,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
} ArrowDirection;

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
    int id;
    const char **items;
    int item_count;
    int *selected_index;
    int *selected;
    int *selected_count;
    int *anchor;
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
} TreeItem;

typedef struct {
    Rectangle bounds;
    int id;
    const TreeItem *items;
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
} AccessibilityNode;

int CanvasHitTest(Vector2 point, Rectangle *items, int item_count);
Vector2 CanvasToScreen(Canvas canvas, Vector2 point);
Rectangle CanvasRectToScreen(Canvas canvas, Rectangle rect);

int AcceleratorPressed(Accelerator accelerator);
int DispatchAccelerators(const Accelerator *accelerators, int count);
MenuResult Menu(MenuProps menu);
int SetClipboardTextValue(const char *text);
const char *GetClipboardTextValue(void);
int SetPrimarySelectionTextValue(const char *text);
const char *GetPrimarySelectionTextValue(void);
int ClipboardSourceHasText(ClipboardSource source);
const char *GetClipboardSourceText(const ClipboardBuffer *clipboard,
                                     ClipboardSource source);
int SetPrimarySelectionFromText(const char *text);
int CopySelectionTextToClipboard(ClipboardBuffer *clipboard,
                                   const char *text);
int ClipboardTargetIncludes(const char *target, char wanted);
int ClipboardTargetUsesPrimary(const char *target);
const char *GetClipboardTargetText(const ClipboardBuffer *clipboard,
                                     const char *target);
int RequestClipboardTargetWrite(ClipboardBuffer *clipboard,
                                  const char *target, const char *text);
int HandleClipboardOSC52(ClipboardBuffer *clipboard, const char *payload,
                           ClipboardOSC52WriteFn write_response,
                           void *userdata);
int WriteClipboardPaste(const char *text, int bracketed,
                          ClipboardPasteWriteFn write_text,
                          void *userdata);
int WriteClipboardTextPaste(ClipboardBuffer *clipboard, const char *text,
                              int bracketed, ClipboardPasteWriteFn write_text,
                              void *userdata);
int WriteClipboardSourcePaste(ClipboardBuffer *clipboard,
                                ClipboardSource source, int bracketed,
                                ClipboardPasteWriteFn write_text,
                                void *userdata);
void InitClipboardBuffer(ClipboardBuffer *buffer, const char *text);
int SetClipboardBufferText(ClipboardBuffer *buffer, const char *text);
int RequestClipboardBufferWrite(ClipboardBuffer *buffer, const char *text);
const char *GetClipboardBufferText(const ClipboardBuffer *buffer);
int ClipboardBufferHasPendingWrite(const ClipboardBuffer *buffer);
int SyncClipboardBufferFromHost(ClipboardBuffer *buffer);
int FlushClipboardBufferToHost(ClipboardBuffer *buffer);
#endif
