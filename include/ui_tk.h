#ifndef KRYON_TK_H
#define KRYON_TK_H

#include "kryon_compat.generated.h"
#include "ui_canvas_props.generated.h"
#include "ui_collapsible_props.generated.h"
#include "ui_color_picker_props.generated.h"
#include "ui_drag_drop_props.generated.h"
#include "ui_drag_props.generated.h"
#include "ui_dropdown_props.generated.h"
#include "ui_fieldset_props.generated.h"
#include "ui_input_props.generated.h"
#include "ui_list_box_props.generated.h"
#include "ui_menu_props.generated.h"
#include "ui_paned_view_props.generated.h"
#include "ui_plot_props.generated.h"
#include "ui_popup_props.generated.h"
#include "ui_progress_props.generated.h"
#include "ui_radio_props.generated.h"
#include "ui_scroll_props.generated.h"
#include "ui_separator_props.generated.h"
#include "ui_slider_props.generated.h"
#include "ui_spinbox_props.generated.h"
#include "ui_table_view_props.generated.h"
#include "ui_text_props.generated.h"
#include "ui_toggle_props.generated.h"
#include "ui_tree_view_props.generated.h"
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
    ARROW_LEFT = 0,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
} ArrowDirection;

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
