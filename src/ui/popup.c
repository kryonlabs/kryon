#include "ui_internal.h"
#include "ui_paint_layers_internal.h"
#include "ui_tree_layout_internal.h"
#include "ui_disabled_internal.h"
#include "ui_input_clip_internal.h"
#include "ui_style_internal.h"
#include "runtime/popup_policy.h"
#include <stdlib.h>

typedef struct ComposedPopupScope {
    struct ComposedPopupScope *previous;
    PaintLayers *layers;
    PaintLayerToken paint;
    PopupInputToken input;
    TreeLayoutScopeState layout;
    DisabledScopeState disabled;
    InputClipScopeState input_clip;
    PopupLifecycle state;
    int id, has_paint, has_input, has_clip;
    bool local_open;
    bool *open;
} ComposedPopupScope;

static ComposedPopupScope *popup_scope;

static void
apply_popup_state(PopupLifecycle state, bool *open, PopupInput *context, int id)
{
    if (state.eligible && !state.decision.tooltip && open) {
        *open = state.open;
    }
    if (state.close_input && context) {
        ui_popup_input_close(context, id);
    }
}

static int
enter_popup_scope(PopupProps popup, PopupLifecycle state, PaintLayers *layers)
{
    PopupInput *context = state.decision.captures_input ?
        (layers ? ui_paint_layers_input(layers) : ui_popup_input_bound()) : NULL;
    PopupInputToken input = {0};
    if (context) {
        input = ui_popup_input_begin(context, popup.id, state.input_bounds);
    }
    state = PopupLifecycleKeyboard(state, IsKeyPressed(KEY_ESCAPE) != 0,
                                   ui_popup_input_keyboard_captures() != 0);
    apply_popup_state(state, popup.open, context, popup.id);
    if (!state.visible) {
        if (context) {
            ui_popup_input_end(input);
        }
        return 0;
    }

    ComposedPopupScope *scope = calloc(1, sizeof(*scope));
    if (!scope) {
        abort();
    }
    scope->previous = popup_scope;
    scope->layers = layers;
    scope->input = input;
    scope->has_input = context != NULL;
    scope->state = state;
    scope->id = popup.id;
    scope->local_open = state.open;
    scope->open = state.decision.tooltip ? &scope->local_open : popup.open;
    if (layers) {
        scope->paint = ui_paint_layer_begin(layers, popup.id);
        scope->has_paint = 1;
    } else {
        scope->layout = ui_tree_layout_suspend();
        scope->disabled = ui_disabled_suspend();
        scope->input_clip = ui_input_clip_suspend();
    }
    if (state.backdrop_alpha > 0) {
        SetModalCapture(popup.bounds);
        if (IsWindowReady()) {
            DrawRectangle(0, 0, GetViewWidth(), GetViewHeight(),
                          (Color){0, 0, 0, (unsigned char)state.backdrop_alpha});
        }
    }
    PushInputClip(popup.bounds);
    if (IsWindowReady()) {
        BeginClip((int)popup.bounds.x, (int)popup.bounds.y,
                  (int)popup.bounds.width, (int)popup.bounds.height);
        Style panel = ui_unpack_style(ui_control_style_frame_role_kind(
            (ButtonProps){.tone = ButtonToneNeutral,
                          .emphasis = ButtonEmphasisSoft,
                          .class_name = popup.class_name},
            ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
            StyleKindPopup(), PopupPanelRole()).value);
        ui_draw_material(popup.bounds, (Rectangle){0}, panel.background, panel.border,
                         panel.border, panel.radius, panel.border_width,
                         0, 0, 0, panel.focus, 0, panel.opacity,
                         ui_style_fill(panel), panel.material);
        scope->has_clip = 1;
    }
    popup_scope = scope;
    return 1;
}

static void
finish_popup_scope(ComposedPopupScope *scope, bool close_requested)
{
    scope->state = PopupLifecycleFinish(scope->state, *scope->open, close_requested);
    *scope->open = scope->state.open;
    if (!scope->state.visible && scope->layers) {
        ui_paint_layers_hide(scope->layers, scope->id);
    } else if (scope->state.close_input && scope->has_input) {
        ui_popup_input_close(scope->input.context, scope->id);
    }
}

static void
end_popup_scope(void)
{
    ComposedPopupScope *scope = popup_scope;
    if (!scope) {
        abort();
    }
    finish_popup_scope(scope, false);
    if (scope->has_clip) {
        EndClip();
    }
    PopInputClip();
    if (scope->has_paint) {
        ui_paint_layer_end(scope->paint);
    } else {
        ui_input_clip_resume(scope->input_clip);
        ui_disabled_resume(scope->disabled);
        ui_tree_layout_resume(scope->layout);
    }
    if (scope->has_input) {
        ui_popup_input_end(scope->input);
    }
    popup_scope = scope->previous;
    free(scope);
}

int PopupScope(PopupProps popup)
{
    PaintLayers *layers = ui_frame_paint_layers();
    Vector2 mouse = ui_mouse_world();
    PopupLifecycle state = PopupLifecycleBegin(popup.flags, (PopupFrameInput){
        .id = popup.id,
        .bounds = popup.bounds,
        .trigger = popup.trigger,
        .has_open = popup.open != NULL,
        .open = popup.open ? *popup.open : false,
        .disabled = popup.disabled != 0,
        .trigger_blocked = InputCapturesClick(mouse) != 0,
        .mouse = mouse,
        .right_released = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) != 0,
        .view_width = GetViewWidth(),
        .view_height = GetViewHeight(),
    });
    if (!state.decision.valid) {
        abort();
    }
    if (!state.eligible) {
        return 0;
    }
    PopupInput *context = layers ? ui_paint_layers_input(layers) :
                                  ui_popup_input_bound();
    state = PopupLifecycleRelease(state,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, ReleaseConsumed() != 0,
        CheckCollisionPointRec(mouse, popup.bounds) != 0);
    if (state.consume_release) {
        ConsumeRelease();
    }
    apply_popup_state(state, popup.open, context, popup.id);
    if (!state.visible) {
        return 0;
    }
    return enter_popup_scope(popup, state, layers);
}

void popup_close_scope(void)
{
    if (!popup_scope) {
        abort();
    }
    finish_popup_scope(popup_scope, true);
}

void PopupEndScope(void)
{
    end_popup_scope();
}
