#include "ui_internal.h"
#include "ui_image_internal.h"

void
RenderTutorialImagePlaceholder(const char *label, int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, DarkenUIColor(c_bg, 12));
    RenderBevel(x, y, w, h, DarkenUIColor(c_bg, 45), LightenUIColor(c_bg, 35));
    int font = GetFontSize();
    int tw = TextWidth(label, font);
    RenderText(label, x + w / 2 - tw / 2, GetUIControlTextY(label, y, h, font), font, c_text);
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
