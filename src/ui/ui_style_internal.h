#ifndef UI_STYLE_INTERNAL_H
#define UI_STYLE_INTERNAL_H

#include "ui_controls.h"
#include "runtime/surface.h"
#include "runtime/style.h"

StyleFrame ui_button_style_frame(ButtonProps button, ButtonState state,
    int automatic, float h, float p, float f);

/* Representation adapters; transition and field-presence policy is .kry. */
Style ui_unpack_style(StyleData value);
StyleStates ui_pack_style_states(ControlStyle control);
Style ui_style_transition(Style resolved, Style normal, Style hover,
                          Style press, Style focus, float h, float p, float f,
                          FillStates *fill);
FillStates ui_style_fill(Style value);
StyleData ui_style_apply_effects_data(StyleData value);
StyleFrame ui_style_apply_effects_frame(StyleFrame frame);
Style ui_style_apply_effects(Style value);
FillStates ui_style_apply_effects_fill(FillStates fill);

/* Host drawing only; material geometry and interaction are generated .kry. */
Rectangle ui_draw_material(Rectangle bounds, Rectangle surface_bounds,
    Color background, Color border, Color light, float radius, float border_width,
    float hover, float press, int disabled, Color focus, float focused,
    float opacity, FillStates fill_states, MaterialKind material);

#endif
