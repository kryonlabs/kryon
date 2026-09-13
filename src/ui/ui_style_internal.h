#ifndef UI_STYLE_INTERNAL_H
#define UI_STYLE_INTERNAL_H

#include "ui_controls.h"
#include "ui_style_sheet.h"
#include "runtime/surface.h"
#include "runtime/style.h"
#include "runtime/theme.h"

StyleFrame ui_button_style_frame(ButtonProps button, ButtonState state,
    int automatic, float h, float p, float f);
StyleFrame ui_control_style_frame_kind(ButtonProps button, ButtonState state,
    int automatic, float h, float p, float f, int style_kind);
StyleFrame ui_control_style_frame_role_kind(ButtonProps button, ButtonState state,
    int automatic, float h, float p, float f, int style_kind, int role);

/* Representation adapters; transition and field-presence policy is .kry. */
Style ui_unpack_style(StyleData value);
StyleStates ui_pack_style_states(ControlStyle control);
Style ResolveControlStyle(Style base, ControlStyle control,
                          ButtonState state);
Style ui_style_transition(Style resolved, Style normal, Style hover,
                          Style press, Style focus, float h, float p, float f,
                          FillStates *fill);
FillStates ui_style_fill(Style value);
StyleData ui_style_apply_effects_data(StyleData value);
StyleFrame ui_style_apply_effects_frame(StyleFrame frame);
Style ui_style_apply_effects(Style value);
FillStates ui_style_apply_effects_fill(FillStates fill);
Style ui_app_style(void);
Style ui_surface_style(void);

/* Theme adapters only; widget visual policy belongs to runtime .kry. */
void ui_runtime_theme_values(Palette *palette_out, Metrics *metrics_out);

/* Host drawing only; material geometry and interaction are generated .kry. */
Rectangle ui_draw_material(Rectangle bounds, Rectangle surface_bounds,
    Color background, Color border, Color light, float radius, float border_width,
    float hover, float press, int disabled, Color focus, float focused,
    float opacity, FillStates fill_states, MaterialKind material);

#endif
