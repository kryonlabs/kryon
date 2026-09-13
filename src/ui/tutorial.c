#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_image_internal.h"
#include "runtime/image.h"
#include "runtime/style.h"

static Style
tutorial_image_style(int role)
{
    return ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindImage(), role).value);
}

void
RenderTutorialImagePlaceholder(const char *label, int x, int y, int w, int h)
{
    Style image = tutorial_image_style(StyleAny());
    Style text = tutorial_image_style(6);
    int font = ImagePlaceholderFontFor(
        GetFontSize(), StyleFontValue(text.fields, text.font_size));
    int font_token = PushTextFont(text.typeface);
    int tw = TextWidth(label, font);
    ImagePlaceholderLayout layout =
        ImagePlaceholderLayoutFor((Rectangle){(float)x, (float)y,
                                  (float)w, (float)h}, tw, font);

    ui_draw_material(layout.bounds, (Rectangle){0}, image.background,
                     image.border, image.border, image.radius,
                     image.border_width, 0.0f, 0.0f, 0, image.focus,
                     0.0f, image.opacity, ui_style_fill(image),
                     image.material);
    RenderText(label, layout.label_x, layout.label_y, font,
               Fade(text.foreground, text.opacity));
    PopTextFont(font_token);
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
    image.fit = ImageFitCover;
    ImageTexture(texture, image);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */
