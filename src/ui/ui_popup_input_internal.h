#ifndef KRYON_UI_POPUP_INPUT_INTERNAL_H
#define KRYON_UI_POPUP_INPUT_INTERNAL_H
#include "kryon.h"
typedef struct UIPopupInput UIPopupInput;
typedef struct UIPopupInputToken {
    UIPopupInput *context;
    unsigned long generation;
    unsigned long order;
    int owner;
} UIPopupInputToken;
typedef struct UIPopupInputOwner {
    UIPopupInput *context;
    int owner;
    int has_owner;
} UIPopupInputOwner;
UIPopupInput *ui_popup_input_create(void);
void ui_popup_input_destroy(UIPopupInput *context);
UIPopupInput *ui_popup_input_bind(UIPopupInput *context);
UIPopupInput *ui_popup_input_bound(void);
void ui_popup_input_frame(UIPopupInput *context);
void ui_popup_input_finish(UIPopupInput *context);
void ui_popup_input_retire_missing(UIPopupInput *context);
UIPopupInputToken ui_popup_input_begin(UIPopupInput *context, int owner, Rectangle bounds);
void ui_popup_input_end(UIPopupInputToken token);
void ui_popup_input_close(UIPopupInput *context, int owner);
int ui_popup_input_captures(UIPopupInput *context, Vector2 point);
int ui_popup_input_current_captures(Vector2 point);
int ui_popup_input_keyboard_captures(void);
int ui_popup_input_keyboard_was_captured(void);
int ui_popup_input_snapshot_keyboard_captures(UIPopupInputToken token);
void ui_popup_input_register_focus(int id, UIPopupInputToken token, int eligible);
int ui_popup_input_focus_captures(int id);
/* Persistent owner identity for pointer interactions spanning frames. */
UIPopupInputOwner ui_popup_input_owner(void);
int ui_popup_input_owner_captures(UIPopupInputOwner owner);
/* Declaration snapshots do not reopen lexical scopes during deferred routing. */
UIPopupInputToken ui_popup_input_snapshot(void);
int ui_popup_input_snapshot_captures(UIPopupInputToken token, Vector2 point);
int ui_input_captures_snapshot(Vector2 point, UIPopupInputToken token);
int ui_register_focus_snapshot(int id, Rectangle bounds, UIPopupInputToken token);
#endif
