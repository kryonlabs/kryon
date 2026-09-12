#include "ui_internal.h"
#include "ui_paint_layers_internal.h"
#include "ui_tree_layout_internal.h"
#include "ui_disabled_internal.h"
#include "ui_input_clip_internal.h"
#include "runtime/popup_policy.h"
#include <limits.h>
#include <stdlib.h>

typedef struct UIComposedPopupScope {
    struct UIComposedPopupScope *previous;
    UIPaintLayers *layers;
    UIPaintLayerToken paint;
    UIPopupInputToken input;
    UITreeLayoutScope layout;
    UIDisabledScope disabled;
    UIInputClipScope input_clip;
    Rectangle popup;
    int id, has_paint, has_input, has_clip;
    bool local_open;
    bool *open;
} UIComposedPopupScope;

static UIComposedPopupScope *popup_scope;

static int
enter_popup_scope(int id, bool *open, Rectangle popup,
                  UIPaintLayers *layers, UIPopupInputToken input,
                  int capture_input, Rectangle input_bounds, int backdrop)
{
    UIPopupInput *context = capture_input ?
        (layers ? ui_paint_layers_input(layers) : ui_popup_input_bound()) : NULL;
    if(context && !input.context)
        input = ui_popup_input_begin(context,id,input_bounds);
    if(capture_input && IsKeyPressed(KEY_ESCAPE) &&
       !ui_popup_input_keyboard_captures()) {
        *open = false;
        if(context) {
            ui_popup_input_close(context,id);
            ui_popup_input_end(input);
        }
        return 0;
    }

    UIComposedPopupScope *scope = calloc(1,sizeof(*scope));
    if(!scope) abort();
    scope->previous = popup_scope;
    scope->layers = layers;
    scope->input = input;
    scope->has_input = context != NULL;
    scope->popup = popup;
    scope->id = id;
    scope->local_open = true;
    scope->open = open ? open : &scope->local_open;
    if(layers) {
        scope->paint = ui_paint_layer_begin(layers,id);
        scope->has_paint = 1;
    } else {
        scope->layout = ui_tree_layout_suspend();
        scope->disabled = ui_disabled_suspend();
        scope->input_clip = ui_input_clip_suspend();
    }
    if(backdrop) SetUIModalCapture(popup);
    if(backdrop && IsWindowReady())
        DrawRectangle(0,0,GetUIViewWidth(),GetUIViewHeight(),
                      (Color){0,0,0,180});
    PushUIInputClip(popup);
    if(IsWindowReady()) {
        BeginUIClip((int)popup.x,(int)popup.y,(int)popup.width,(int)popup.height);
        DrawRectangleRec(popup,GetThemeSurface());
        scope->has_clip = 1;
    }
    popup_scope = scope;
    return 1;
}

static void close_popup_scope(UIComposedPopupScope *scope)
{
    *scope->open = false;
    if(scope->layers) ui_paint_layers_hide(scope->layers,scope->id);
    else if(scope->has_input)
        ui_popup_input_close(scope->input.context,scope->id);
}

static void end_popup_scope(void)
{
    UIComposedPopupScope *scope = popup_scope;
    if(!scope) abort();
    if(!*scope->open) close_popup_scope(scope);
    if(scope->has_clip) EndUIClip();
    PopUIInputClip();
    if(scope->has_paint) ui_paint_layer_end(scope->paint);
    else {
        ui_input_clip_resume(scope->input_clip);
        ui_disabled_resume(scope->disabled);
        ui_tree_layout_resume(scope->layout);
    }
    if(scope->has_input) ui_popup_input_end(scope->input);
    popup_scope = scope->previous;
    free(scope);
}

int BeginPopup(PopupProps popup)
{
    PopupDecision decision = PopupDecisionFor(popup.flags, popup.disabled != 0);
    if(!decision.valid) abort();
    if(!PopupCanBegin(decision, popup.id, popup.bounds, popup.trigger,
                      popup.open != NULL))
        return 0;
    UIPaintLayers *layers = ui_frame_paint_layers();
    UIPopupInput *input_context = layers ? ui_paint_layers_input(layers) :
                                          ui_popup_input_bound();
    if(decision.context && !popup.disabled &&
       IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) &&
       CheckCollisionPointRec(ui_mouse_world(),popup.trigger) &&
       !UIInputCapturesClick(ui_mouse_world()))
        *popup.open = true;
    if(decision.tooltip) {
        if(popup.disabled ||
           !CheckCollisionPointRec(ui_mouse_world(),popup.trigger)) return 0;
    } else {
        *popup.open = PopupOpenAfterDisabled(decision, *popup.open,
                                             popup.disabled != 0);
        if(!*popup.open) {
            if(input_context) ui_popup_input_close(input_context,popup.id);
            return 0;
        }
    }
    if(!decision.tooltip && !decision.modal &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !UIReleaseConsumed() &&
       !CheckCollisionPointRec(ui_mouse_world(),popup.bounds)) {
        UIConsumeRelease();
        *popup.open = false;
        if(input_context) ui_popup_input_close(input_context,popup.id);
        return 0;
    }
    Rectangle input_bounds = PopupInputBounds(decision, popup.bounds,
                                              GetUIViewWidth(),
                                              GetUIViewHeight());
    return enter_popup_scope(popup.id,decision.tooltip ? NULL : popup.open,popup.bounds,
                             layers,
                             (UIPopupInputToken){0},decision.captures_input,
                             input_bounds,PopupBackdropAlpha(decision) > 0);
}

void ClosePopup(void)
{
    if(!popup_scope) abort();
    close_popup_scope(popup_scope);
}

void EndPopup(void)
{
    end_popup_scope();
}
