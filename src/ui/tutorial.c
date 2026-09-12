#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_image_internal.h"
#include "runtime/image.h"

void
RenderTutorialImagePlaceholder(const char *label, int x, int y, int w, int h)
{
    int font = GetFontSize();
    int tw = TextWidth(label, font);
    ImagePlaceholderLayout layout =
        ImagePlaceholderLayoutFor((Rectangle){(float)x, (float)y,
                                  (float)w, (float)h}, tw, font);
    Style surface = ui_surface_style();
    Style text = ui_resolve_button_style_kind((ButtonProps){0},
                                              ButtonStateNormal,
                                              StyleKindText());
    ui_draw_material(layout.bounds, (Rectangle){0}, surface.background,
                     surface.border, surface.border, surface.radius,
                     surface.border_width, 0.0f, 0.0f, 0, surface.focus,
                     0.0f, surface.opacity, ui_style_fill(surface),
                     surface.material);
    RenderText(label, layout.label_x, layout.label_y, font, text.foreground);
}

void
RenderTutorialImage(Texture2D texture, const char *fallback, int x, int y, int w, int h)
{
    ImageProps image = {0};

    if(texture.id == 0) {
        RenderTutorialImagePlaceholder(fallback, x, y, w, h);
        return;
    }

    image.bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    image.fit = IMAGE_FIT_COVER;
    image.tint = WHITE;
    image.style.enabled = 1;
    ImageTexture(texture, image);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */
