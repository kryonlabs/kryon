#include "ui_internal.h"
#include "ui_paint_layers_internal.h"
#include "ui_tree_layout_internal.h"
#include "ui_disabled_internal.h"
#include "ui_input_clip_internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct UIComposedPopupScope {
    struct UIComposedPopupScope *previous;
    UIPaintLayers *layers;
    UIPaintLayerToken paint;
    UIPopupInputToken input;
    UITreeLayoutScope layout;
    UIDisabledScope disabled;
    UIInputClipScope input_clip;
    Rectangle popup;
    int id, kind, has_paint, has_input, has_clip;
    bool local_open;
    bool *open;
} UIComposedPopupScope;

enum { UI_COMPOSED_COMBO, UI_COMPOSED_POPUP };
static UIComposedPopupScope *popup_scope;

static int
enter_popup_scope(int id, bool *open, Rectangle popup, int kind,
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
    scope->kind = kind;
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

static Rectangle combo_popup(ComboProps combo)
{
    Rectangle popup = {combo.bounds.x,combo.bounds.y+combo.bounds.height,
                       combo.popup_size.x,combo.popup_size.y};
    float row = combo.bounds.height > 0 ? combo.bounds.height : 28;
    unsigned heights = combo.flags & (ComboHeightSmall|ComboHeightRegular|
                                      ComboHeightLarge|ComboHeightLargest);
    if(heights && (heights & (heights-1))) abort();
    if(popup.width <= 0) popup.width = combo.bounds.width;
    if(popup.height <= 0) {
        int rows = (combo.flags & ComboHeightSmall) ? 4 :
                   (combo.flags & ComboHeightLarge) ? 20 :
                   (combo.flags & ComboHeightLargest) ? 32 : 8;
        popup.height = row*rows;
    }
    int width = GetScreenWidth(), height = GetScreenHeight();
    if(width > 0) {
        if(popup.width > width) popup.width = (float)width;
        popup.x = (combo.flags & ComboPopupAlignLeft) ? combo.bounds.x :
                  combo.bounds.x+combo.bounds.width-popup.width;
        if(popup.x < 0) popup.x = 0;
        if(popup.x+popup.width > width) popup.x = width-popup.width;
    }
    if(height > 0) {
        if(popup.height > height) popup.height = (float)height;
        if(popup.y+popup.height > height)
            popup.y = combo.bounds.y-popup.height;
        if(popup.y < 0) popup.y = 0;
        if(popup.y+popup.height > height) popup.height = height-popup.y;
    }
    return popup;
}

int BeginCombo(ComboProps combo)
{
    if(combo.id <= 0 || combo.open == NULL) return 0;
    Rectangle popup = combo_popup(combo);
    int was_open = *combo.open != 0;
    UIPaintLayers *layers = ui_frame_paint_layers();
    UIPopupInput *context = layers ? ui_paint_layers_input(layers) :
                                    ui_popup_input_bound();
    UIPopupInputToken input = {0};
    if(combo.disabled) *combo.open = 0;

    if(was_open && !combo.disabled &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(ui_mouse_world(),popup)) {
        UIConsumeRelease();
        *combo.open = 0;
        was_open = 0;
    }

    if(*combo.open && context)
        input = ui_popup_input_begin(context,combo.id,popup);

    Rectangle trigger = combo.bounds;
    const char *preview = (combo.flags & ComboNoPreview) ? "" :
                          (combo.preview ? combo.preview : "");
    if(combo.flags & ComboWidthFitPreview) {
        float need = (float)TextWidth(preview,GetFontSize())+20;
        if(!(combo.flags & ComboNoArrowButton)) need += 20;
        if(need > trigger.width) trigger.width = need;
    }
    size_t n = strlen(preview);
    char *label = malloc(n+4);
    if(!label) abort();
    if(combo.flags & ComboNoArrowButton) memcpy(label,preview,n+1);
    else snprintf(label,n+4,"%s v",preview);
    int pressed = Button((ButtonProps){.bounds=trigger,.label=label,
                         .style=ButtonStyleSecondary,.id=combo.id,
                         .disabled=combo.disabled});
    free(label);
    if(pressed) *combo.open = !*combo.open;
    if(!*combo.open) {
        if(context && input.context) {
            ui_popup_input_close(context,combo.id);
            ui_popup_input_end(input);
        }
        return 0;
    }
    return enter_popup_scope(combo.id,combo.open,popup,UI_COMPOSED_COMBO,
                             layers,input,1,popup,0);
}

static void close_popup_scope(UIComposedPopupScope *scope)
{
    *scope->open = false;
    if(scope->layers) ui_paint_layers_hide(scope->layers,scope->id);
    else if(scope->has_input)
        ui_popup_input_close(scope->input.context,scope->id);
}

static void end_popup_scope(int kind)
{
    UIComposedPopupScope *scope = popup_scope;
    if(!scope || scope->kind != kind) abort();
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

void CloseCombo(void)
{
    if(!popup_scope || popup_scope->kind != UI_COMPOSED_COMBO) abort();
    close_popup_scope(popup_scope);
}

void EndCombo(void)
{
    end_popup_scope(UI_COMPOSED_COMBO);
}

int BeginPopup(PopupProps popup)
{
    int tooltip = (popup.flags & PopupTooltip) != 0;
    int modal = (popup.flags & PopupModal) != 0;
    int is_context = (popup.flags & PopupContext) != 0;
    if(popup.flags & ~((unsigned int)(PopupTooltip|PopupModal|PopupContext))) abort();
    if((tooltip && (modal || is_context)) || (modal && is_context)) abort();
    if(popup.id <= 0 || popup.bounds.width <= 0 || popup.bounds.height <= 0 ||
       (!tooltip && popup.open == NULL) ||
       ((tooltip || is_context) &&
        (popup.trigger.width <= 0 || popup.trigger.height <= 0)))
        return 0;
    UIPaintLayers *layers = ui_frame_paint_layers();
    UIPopupInput *input_context = layers ? ui_paint_layers_input(layers) :
                                          ui_popup_input_bound();
    if(is_context && !popup.disabled &&
       IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) &&
       CheckCollisionPointRec(ui_mouse_world(),popup.trigger) &&
       !UIInputCapturesClick(ui_mouse_world()))
        *popup.open = true;
    if(tooltip) {
        if(popup.disabled ||
           !CheckCollisionPointRec(ui_mouse_world(),popup.trigger)) return 0;
    } else {
        if(popup.disabled) *popup.open = false;
        if(!*popup.open) {
            if(input_context) ui_popup_input_close(input_context,popup.id);
            return 0;
        }
    }
    if(!tooltip && !modal &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !UIReleaseConsumed() &&
       !CheckCollisionPointRec(ui_mouse_world(),popup.bounds)) {
        UIConsumeRelease();
        *popup.open = false;
        if(input_context) ui_popup_input_close(input_context,popup.id);
        return 0;
    }
    Rectangle input_bounds = popup.bounds;
    if(modal)
        input_bounds = (Rectangle){0,0,GetUIViewWidth(),GetUIViewHeight()};
    return enter_popup_scope(popup.id,tooltip ? NULL : popup.open,popup.bounds,
                             UI_COMPOSED_POPUP,layers,
                             (UIPopupInputToken){0},!tooltip,input_bounds,modal);
}

void ClosePopup(void)
{
    if(!popup_scope || popup_scope->kind != UI_COMPOSED_POPUP) abort();
    close_popup_scope(popup_scope);
}

void EndPopup(void)
{
    end_popup_scope(UI_COMPOSED_POPUP);
}
