#include "ui_internal.h"
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
    DrawRectangleRec(layout.bounds, DarkenColor(c_bg, 12));
    RenderBevel((int)layout.bounds.x, (int)layout.bounds.y,
                (int)layout.bounds.width, (int)layout.bounds.height,
                DarkenColor(c_bg, 45), LightenColor(c_bg, 35));
    RenderText(label, layout.label_x, layout.label_y, font, c_text);
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
