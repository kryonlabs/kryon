#ifndef KRYON_UI_POPUP_INPUT_INTERNAL_H
#define KRYON_UI_POPUP_INPUT_INTERNAL_H
#include "kryon.h"
typedef struct PopupInput PopupInput;
typedef struct PopupInputToken {
    PopupInput *context;
    unsigned long generation;
    unsigned long order;
    int owner;
} PopupInputToken;
typedef struct PopupInputOwner {
    PopupInput *context;
    int owner;
    int has_owner;
} PopupInputOwner;
PopupInput *ui_popup_input_create(void);
void ui_popup_input_destroy(PopupInput *context);
PopupInput *ui_popup_input_bind(PopupInput *context);
PopupInput *ui_popup_input_bound(void);
void ui_popup_input_frame(PopupInput *context);
void ui_popup_input_finish(PopupInput *context);
void ui_popup_input_retire_missing(PopupInput *context);
PopupInputToken ui_popup_input_begin(PopupInput *context, int owner, Rectangle bounds);
void ui_popup_input_end(PopupInputToken token);
void ui_popup_input_close(PopupInput *context, int owner);
int ui_popup_input_captures(PopupInput *context, Vector2 point);
int ui_popup_input_current_captures(Vector2 point);
int ui_popup_input_keyboard_captures(void);
int ui_popup_input_keyboard_was_captured(void);
int ui_popup_input_snapshot_keyboard_captures(PopupInputToken token);
void ui_popup_input_register_focus(int id, PopupInputToken token, int eligible);
int ui_popup_input_focus_captures(int id);
/* Persistent owner identity for pointer interactions spanning frames. */
PopupInputOwner ui_popup_input_owner(void);
int ui_popup_input_owner_captures(PopupInputOwner owner);
/* Declaration snapshots do not reopen lexical scopes during deferred routing. */
PopupInputToken ui_popup_input_snapshot(void);
int ui_popup_input_snapshot_captures(PopupInputToken token, Vector2 point);
int ui_input_captures_snapshot(Vector2 point, PopupInputToken token);
int ui_register_focus_snapshot(int id, Rectangle bounds, PopupInputToken token);
#endif
